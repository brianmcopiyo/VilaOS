#!/usr/bin/env bash
# Build a Debian live ISO that boots into Chromium kiosk + React UI.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
KB="$(cd "$SCRIPT_DIR/.." && pwd)"
# Rootfs MUST live on a native Linux FS — /mnt/c breaks debootstrap (no perms/symlinks).
CACHE="${TERMINAL_LIVE_CACHE:-$HOME/.cache/terminal-os-live}"
ROOTFS="$CACHE/rootfs"
ISO_STAGE="$CACHE/iso_stage"
OUT_DIR="$KB/out"
ISO_OUT="$OUT_DIR/terminal.iso"
UI_SRC="$KB/iso_root/ui"
OVERLAY="$KB/overlay"
MARKER="$CACHE/.packages.ok"
DEBIAN_MIRROR="${DEBIAN_MIRROR:-http://deb.debian.org/debian}"
SUITE="${SUITE:-bookworm}"

log() { printf '==> %s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

need_cmd() { command -v "$1" >/dev/null 2>&1 || die "missing command: $1"; }

need_cmd debootstrap
need_cmd mksquashfs
need_cmd grub-mkrescue
need_cmd xorriso
need_cmd rsync

# Ensure Debian archive keyring exists on the host for signature checks.
if [[ ! -f /usr/share/keyrings/debian-archive-keyring.gpg ]]; then
  log "Installing debian-archive-keyring on WSL host…"
  sudo apt-get update -qq
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq debian-archive-keyring
fi

[[ -d "$UI_SRC" ]] || die "React UI missing at $UI_SRC — build UI first (UI_TARGET=iso)"
[[ -f "$UI_SRC/index.html" ]] || die "React UI incomplete: no index.html in $UI_SRC"

mkdir -p "$CACHE" "$OUT_DIR" "$ISO_STAGE/live" "$ISO_STAGE/boot/grub"
log "Live cache (Linux FS): $CACHE"

# --- rootfs -----------------------------------------------------------------
if [[ ! -f "$ROOTFS/etc/debian_version" ]]; then
  log "debootstrap $SUITE → $ROOTFS (first run; several minutes)"
  sudo rm -rf "$ROOTFS"
  sudo mkdir -p "$ROOTFS"
  sudo debootstrap --variant=minbase --include=ca-certificates,sudo,dbus \
    "$SUITE" "$ROOTFS" "$DEBIAN_MIRROR"
  sudo rm -f "$MARKER"
fi

mount_chroot() {
  sudo mount -t proc proc "$ROOTFS/proc"
  sudo mount -t sysfs sys "$ROOTFS/sys"
  sudo mount --bind /dev "$ROOTFS/dev"
  sudo mount --bind /dev/pts "$ROOTFS/dev/pts"
}

umount_chroot() {
  sudo umount "$ROOTFS/dev/pts" 2>/dev/null || true
  sudo umount "$ROOTFS/dev" 2>/dev/null || true
  sudo umount "$ROOTFS/sys" 2>/dev/null || true
  sudo umount "$ROOTFS/proc" 2>/dev/null || true
}

trap umount_chroot EXIT

# --- packages + kiosk config -----------------------------------------------
if [[ ! -f "$MARKER" ]]; then
  log "Installing live-boot, Xorg, Chromium, nginx (long)…"
  mount_chroot
  sudo cp /etc/resolv.conf "$ROOTFS/etc/resolv.conf"
  sudo chroot "$ROOTFS" bash -c "
    set -euo pipefail
    export DEBIAN_FRONTEND=noninteractive
    echo 'deb $DEBIAN_MIRROR $SUITE main contrib non-free non-free-firmware' > /etc/apt/sources.list
    echo 'deb $DEBIAN_MIRROR $SUITE-updates main contrib non-free non-free-firmware' >> /etc/apt/sources.list
    echo 'deb http://security.debian.org/debian-security $SUITE-security main contrib non-free non-free-firmware' >> /etc/apt/sources.list
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends \
      linux-image-amd64 \
      live-boot \
      live-boot-initramfs-tools \
      live-tools \
      systemd-sysv \
      initramfs-tools \
      xorg \
      xserver-xorg-video-all \
      xserver-xorg-input-all \
      xinit \
      openbox \
      chromium \
      nginx \
      network-manager \
      fonts-liberation \
      fonts-dejavu-core \
      dbus-x11 \
      pciutils \
      usbutils \
      curl \
      ca-certificates
    apt-get clean
    rm -rf /var/lib/apt/lists/*
  "
  umount_chroot
  sudo touch "$MARKER"
fi

# --- always refresh UI + kiosk overlay -------------------------------------
log "Applying kiosk configuration and React UI"
mount_chroot

sudo mkdir -p "$ROOTFS/opt/terminal/ui"
sudo rsync -a --delete "$UI_SRC"/ "$ROOTFS/opt/terminal/ui"/

if [[ -d "$OVERLAY" ]]; then
  sudo rsync -a "$OVERLAY"/ "$ROOTFS"/
fi

sudo chroot "$ROOTFS" bash -c '
  set -euo pipefail
  export DEBIAN_FRONTEND=noninteractive

  # User
  if ! id kiosk >/dev/null 2>&1; then
    useradd -m -s /bin/bash -G audio,video,input,netdev kiosk
  fi
  echo "kiosk:kiosk" | chpasswd
  echo "kiosk ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/kiosk
  chmod 440 /etc/sudoers.d/kiosk

  # Hostname
  echo "terminal-os" > /etc/hostname
  cat > /etc/hosts <<EOF
127.0.0.1   localhost
127.0.1.1   terminal-os
::1         localhost ip6-localhost ip6-loopback
EOF

  # nginx → React UI
  cat > /etc/nginx/sites-available/terminal <<EOF
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;
    root /opt/terminal/ui;
    index index.html;
    location / {
        try_files \$uri \$uri/ /index.html;
    }
}
EOF
  rm -f /etc/nginx/sites-enabled/default
  ln -sfn /etc/nginx/sites-available/terminal /etc/nginx/sites-enabled/terminal
  systemctl enable nginx.service NetworkManager.service || true

  # Autologin on tty1
  mkdir -p /etc/systemd/system/getty@tty1.service.d
  cat > /etc/systemd/system/getty@tty1.service.d/autologin.conf <<EOF
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin kiosk --noclear %I \$TERM
EOF

  # Start X on login
  cat > /home/kiosk/.bash_profile <<EOF
# Auto-start graphical kiosk on tty1
if [ -z "\$DISPLAY" ] && [ "\$(tty)" = "/dev/tty1" ]; then
  exec startx -- -keeptty >/home/kiosk/.xsession-errors 2>&1
fi
EOF

  cat > /home/kiosk/.xinitrc <<EOF
#!/bin/sh
xset s off -dpms
xset s noblank
exec openbox-session
EOF
  chmod +x /home/kiosk/.xinitrc

  mkdir -p /home/kiosk/.config/openbox
  cat > /home/kiosk/.config/openbox/autostart <<EOF
#!/bin/sh
# Wait for local nginx, then fullscreen Chromium with Terminal UI
for i in 1 2 3 4 5 6 7 8 9 10; do
  curl -fsS http://127.0.0.1/ >/dev/null 2>&1 && break
  sleep 0.5
done
chromium \\
  --kiosk \\
  --no-first-run \\
  --disable-translate \\
  --disable-infobars \\
  --disable-session-crashed-bubble \\
  --disable-features=TranslateUI \\
  --check-for-update-interval=31536000 \\
  --noerrdialogs \\
  --start-maximized \\
  --window-position=0,0 \\
  --user-data-dir=/home/kiosk/.chromium-kiosk \\
  http://127.0.0.1/ &
EOF
  chmod +x /home/kiosk/.config/openbox/autostart

  chown -R kiosk:kiosk /home/kiosk

  # Quiet motd
  : > /etc/motd
  : > /etc/issue

  # Ensure initramfs knows live-boot
  update-initramfs -u
'

umount_chroot
trap - EXIT

# --- squashfs + live ISO layout --------------------------------------------
# Keep empty mount-point dirs (/dev /proc /sys /run /tmp). Excluding them
# with mksquashfs -e removes the directories and live-boot panics with
# "Attempted to kill init" after "mount point does not exist".
log "Preparing mount points + minimal /dev for live-boot"
sudo mkdir -p \
  "$ROOTFS/dev" "$ROOTFS/proc" "$ROOTFS/sys" "$ROOTFS/run" "$ROOTFS/tmp" \
  "$ROOTFS/mnt" "$ROOTFS/media" "$ROOTFS/var/tmp"
# Minimal static nodes (live-build style) so early mount/move works
if [[ ! -e "$ROOTFS/dev/console" ]]; then
  sudo mknod -m 600 "$ROOTFS/dev/console" c 5 1 || true
fi
if [[ ! -e "$ROOTFS/dev/null" ]]; then
  sudo mknod -m 666 "$ROOTFS/dev/null" c 1 3 || true
fi
if [[ ! -e "$ROOTFS/dev/zero" ]]; then
  sudo mknod -m 666 "$ROOTFS/dev/zero" c 1 5 || true
fi
if [[ ! -e "$ROOTFS/dev/tty" ]]; then
  sudo mknod -m 666 "$ROOTFS/dev/tty" c 5 0 || true
fi
sudo chmod 1777 "$ROOTFS/tmp" "$ROOTFS/var/tmp" || true

log "Creating squashfs (this can take a few minutes)…"
SFS="$ISO_STAGE/live/filesystem.squashfs"
sudo rm -f "$SFS"
# Exclude only bulky caches — never exclude /dev /proc /sys /run /tmp themselves.
sudo mksquashfs "$ROOTFS" "$SFS" \
  -comp xz -noappend \
  -wildcards \
  -e "var/cache/apt/archives/*" \
  -e "var/tmp/*" \
  -e "tmp/*"

# Kernel + initrd from rootfs /boot
VMLINUZ="$(sudo bash -c "ls -1 $ROOTFS/boot/vmlinuz-* | sort -V | tail -1")"
INITRD="$(sudo bash -c "ls -1 $ROOTFS/boot/initrd.img-* | sort -V | tail -1")"
[[ -n "$VMLINUZ" && -f "$VMLINUZ" ]] || die "vmlinuz not found in rootfs"
[[ -n "$INITRD" && -f "$INITRD" ]] || die "initrd not found in rootfs"

sudo cp -f "$VMLINUZ" "$ISO_STAGE/live/vmlinuz"
sudo cp -f "$INITRD" "$ISO_STAGE/live/initrd.img"
sudo chmod 644 "$ISO_STAGE/live/vmlinuz" "$ISO_STAGE/live/initrd.img"

cat > "$ISO_STAGE/boot/grub/grub.cfg" <<'EOF'
set timeout=3
set default=0
insmod all_video
insmod gfxterm
terminal_output gfxterm

menuentry "Terminal OS" {
    linux /live/vmlinuz boot=live components noeject username=kiosk hostname=terminal-os
    initrd /live/initrd.img
}
EOF

log "Packing hybrid ISO → $ISO_OUT"
rm -f "$ISO_OUT"
grub-mkrescue -o "$ISO_OUT" "$ISO_STAGE" \
  --compress=xz \
  -V "TERMINAL_OS"

ls -lh "$ISO_OUT"
log "Done."
