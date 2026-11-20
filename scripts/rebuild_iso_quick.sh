#!/usr/bin/env bash
set -euo pipefail

HOST="/mnt/c/Users/HP/Desktop/OS"
WORK="/root/vilaos-work/src"
OLD="/root/phoneos-work/src"

find "${HOST}/src" "${HOST}/include" "${HOST}/assets" "${HOST}/apps" "${HOST}/sdk" "${HOST}/scripts" "${HOST}/shell" -type f \( -name "*.c" -o -name "*.h" -o -name "*.xml" -o -name "*.sh" -o -name "*.kt" -o -name "*.kbc" -o -name "*.js" -o -name "*.css" -o -name "*.html" -o -name "Makefile" -o -name "vilaos-sdk" -o -name "ktc" \) -exec sed -i "s/\r$//" {} + 2>/dev/null || true

mkdir -p "${WORK}/build"
rsync -a --delete \
  --exclude build \
  --exclude ".git" \
  --exclude "*.png" \
  --exclude "/vilaos" \
  --exclude "/vilaos-musl" \
  --exclude "/vilaos-pkg" \
  --exclude "/vilaos-pkg-musl" \
  --exclude "/vilaos-desktop" \
  --exclude "/vilaos-vfb" \
  --exclude "/vilaos-static" \
  --exclude "/phoneos" \
  --exclude "/phoneos-*" \
  "${HOST}/" "${WORK}/"

if [[ -d "${OLD}/build" ]]; then
  for f in bzImage busybox; do
    if [[ -e "${OLD}/build/${f}" && ! -e "${WORK}/build/${f}" ]]; then
      cp -a "${OLD}/build/${f}" "${WORK}/build/${f}"
    fi
  done
fi

ln -sfn /usr/include/linux /usr/include/x86_64-linux-musl/linux
ln -sfn /usr/include/asm-generic /usr/include/x86_64-linux-musl/asm-generic
ln -sfn /usr/include/asm-generic /usr/include/x86_64-linux-musl/asm 2>/dev/null || true
ln -sfn /usr/include/x86_64-linux-gnu/asm /usr/include/x86_64-linux-musl/asm

cd "${WORK}"
bash scripts/build_musl_gfx_deps.sh /root/vilaos-work/musl-gfx || true
make clean
make musl MUSL_GFX_PREFIX=/root/vilaos-work/musl-gfx
strip --strip-unneeded vilaos-musl vilaos-pkg-musl vilaos-musicd-musl
python3 sdk/xaml2react.py shell/xaml shell/web/views || true
bash scripts/pack_apps.sh
bash scripts/pack_gfx_driver.sh
bash scripts/fetch_chromium.sh /root/vilaos-work/chromium || true

if [[ ! -f build/bzImage ]]; then
  echo "missing build/bzImage; run full build first" >&2
  exit 1
fi

BB=""
for cand in build/busybox build/busybox-1.36.1/busybox "${OLD}/build/busybox"; do
  if [[ -x "${cand}" ]]; then
    BB="${cand}"
    break
  fi
done
if [[ -z "${BB}" ]]; then
  echo "missing busybox binary" >&2
  exit 1
fi

install_musl_runtime() {
  local dest="$1"
  mkdir -p "${dest}/lib"
  local musl_ld=""
  for cand in /lib/ld-musl-x86_64.so.1 /usr/lib/x86_64-linux-musl/libc.so /lib/x86_64-linux-musl/libc.so; do
    if [[ -f "${cand}" && -x "${cand}" && "$(stat -c%s "${cand}" 2>/dev/null || echo 0)" -gt 4096 ]]; then
      musl_ld="${cand}"
      break
    fi
  done
  if [[ -z "${musl_ld}" ]]; then
    echo "missing musl dynamic linker" >&2
    exit 1
  fi
  cp -f "${musl_ld}" "${dest}/lib/ld-musl-x86_64.so.1"
  chmod 755 "${dest}/lib/ld-musl-x86_64.so.1"
  ln -sfn ld-musl-x86_64.so.1 "${dest}/lib/libc.so"
}

ROOTFS="${WORK}/build/rootfs"
rm -rf "${ROOTFS}"
mkdir -p "${ROOTFS}"/{bin,sbin,dev,proc,sys,tmp,lib,run/vilaos,assets/screens,assets/values,assets/components,assets/ui,assets/fonts,assets/icons,fonts,icons,res/fonts,sdk/ui,apps,data/apps,var/lib/vilaos,packages,drivers/gfx/builtin,ui,opt}
cp -f "${BB}" "${ROOTFS}/bin/busybox"
for a in sh ls cat mount umount mkdir mknod chmod chown sleep ps free dmesg tar rm ln head cp mv \
  date kill dirname basename test \[ echo printf true false yes no env which find grep sed awk \
  cut tr wc dd sync reboot poweroff halt ifconfig ip route ping wget nc tee tail readlink; do
  ln -sf busybox "${ROOTFS}/bin/${a}"
done
cp -f vilaos-musl "${ROOTFS}/bin/vilaos"
cp -f vilaos-pkg-musl "${ROOTFS}/bin/vilaos-pkg"
cp -f vilaos-musicd-musl "${ROOTFS}/bin/vilaos-musicd"
chmod 755 "${ROOTFS}/bin/vilaos" "${ROOTFS}/bin/vilaos-pkg" "${ROOTFS}/bin/vilaos-musicd"
install_musl_runtime "${ROOTFS}"
echo "builtin" > "${ROOTFS}/drivers/gfx/active"
cat > "${ROOTFS}/drivers/gfx/builtin/manifest.xml" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<driver>
  <id>builtin</id>
  <version>1.0.0</version>
  <abi>1</abi>
  <entry>libvilaos_gfx.so</entry>
  <name>vilaos-gfx-builtin</name>
</driver>
EOF
if [[ -f build/libvilaos_gfx.so ]]; then
  cp -f build/libvilaos_gfx.so "${ROOTFS}/drivers/gfx/builtin/libvilaos_gfx.so"
  cp -f build/libvilaos_gfx.so "${ROOTFS}/lib/libvilaos_gfx.so"
fi
cp -a assets/screens/. "${ROOTFS}/assets/screens/"
if [[ -d assets/values ]]; then cp -a assets/values/. "${ROOTFS}/assets/values/"; fi
if [[ -d assets/components ]]; then cp -a assets/components/. "${ROOTFS}/assets/components/"; fi
if [[ -d assets/ui ]]; then cp -a assets/ui/. "${ROOTFS}/assets/ui/"; fi
if [[ -d sdk/ui ]]; then cp -a sdk/ui/. "${ROOTFS}/sdk/ui/"; cp -a sdk/ui/. "${ROOTFS}/assets/ui/"; fi
if [[ -d assets/fonts ]]; then cp -a assets/fonts/. "${ROOTFS}/assets/fonts/"; fi
if [[ -d fonts ]]; then cp -a fonts/. "${ROOTFS}/fonts/"; cp -a fonts/. "${ROOTFS}/assets/fonts/" 2>/dev/null || true; fi
if [[ -d res/fonts ]]; then cp -a res/fonts/. "${ROOTFS}/res/fonts/"; fi
if [[ -d icons ]]; then
  cp -a icons/. "${ROOTFS}/icons/"
  cp -a icons/. "${ROOTFS}/assets/icons/"
fi
cp -a apps/. "${ROOTFS}/apps/"
mkdir -p "${ROOTFS}/shell"
cp -a shell/. "${ROOTFS}/shell/" 2>/dev/null || true
cp -a build/packages/. "${ROOTFS}/packages/" 2>/dev/null || true
if [[ -d shell/web ]]; then
  mkdir -p "${ROOTFS}/ui"
  cp -a shell/web/. "${ROOTFS}/ui/"
fi
install -m 755 scripts/vilaos-chromium.sh "${ROOTFS}/bin/vilaos-chromium"
mkdir -p "${ROOTFS}/opt/chromium-data" "${ROOTFS}/usr/share/fonts/truetype/dejavu" "${ROOTFS}/etc" "${ROOTFS}/run/user/0" "${ROOTFS}/root"
printf 'root:x:0:0:root:/root:/bin/sh\n' > "${ROOTFS}/etc/passwd"
printf 'root:x:0:\nvideo:x:44:root\n' > "${ROOTFS}/etc/group"
printf 'root::0:0:99999:7:::\n' > "${ROOTFS}/etc/shadow"
chmod 644 "${ROOTFS}/etc/passwd" "${ROOTFS}/etc/group"
chmod 600 "${ROOTFS}/etc/shadow"

if [[ -f /root/vilaos-work/chromium/icudtl.dat ]]; then
  cp -f /root/vilaos-work/chromium/icudtl.dat "${ROOTFS}/opt/chromium-data/icudtl.dat"
  ICU_SZ=$(stat -c%s "${ROOTFS}/opt/chromium-data/icudtl.dat")
  echo "staged icudtl.dat bytes=${ICU_SZ}"
  if [[ "${ICU_SZ}" -lt 10000000 ]]; then
    echo "icudtl.dat too small" >&2
    exit 1
  fi
fi
for f in v8_context_snapshot.bin chrome_100_percent.pak chrome_200_percent.pak resources.pak snapshot_blob.bin; do
  if [[ -f "/root/vilaos-work/chromium/${f}" ]]; then
    cp -f "/root/vilaos-work/chromium/${f}" "${ROOTFS}/opt/chromium-data/${f}"
  fi
done
if [[ -d /root/vilaos-work/chromium/locales ]]; then
  mkdir -p "${ROOTFS}/opt/chromium-data/locales"
  cp -a /root/vilaos-work/chromium/locales/. "${ROOTFS}/opt/chromium-data/locales/" 2>/dev/null || true
fi
bash scripts/stage_wayland_rootfs.sh "${ROOTFS}" || true

if [[ -d /usr/share/fonts/truetype/dejavu ]]; then
  cp -a /usr/share/fonts/truetype/dejavu/. "${ROOTFS}/usr/share/fonts/truetype/dejavu/" 2>/dev/null || true
elif [[ -d /usr/share/fonts/dejavu ]]; then
  cp -a /usr/share/fonts/dejavu/. "${ROOTFS}/usr/share/fonts/truetype/dejavu/" 2>/dev/null || true
fi
if command -v apt-get >/dev/null 2>&1; then
  apt-get install -y -qq fonts-dejavu-core libgbm1 2>/dev/null || true
  cp -a /usr/share/fonts/truetype/dejavu/. "${ROOTFS}/usr/share/fonts/truetype/dejavu/" 2>/dev/null || true
fi

cat > "${ROOTFS}/bin/vilaos-chrome-setup" << "EOF"
#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
BB=/bin/busybox
mkdir -p /mnt/iso /opt /run/vilaos /tmp/chromium-data /tmp/chromium-config /tmp/chromium-cache /dev /dev/dri /usr/share/fonts
i=0
mounted=0
while [ "$i" -lt 30 ]; do
  for d in /dev/sr0 /dev/sr1 /dev/cdrom /dev/dvd /dev/hdb /dev/hdc /dev/hdd; do
    [ -e "$d" ] || continue
    $BB umount /mnt/iso 2>/dev/null || true
    if $BB mount -t iso9660 -o ro,norock "$d" /mnt/iso 2>/dev/null; then
      $BB umount /mnt/iso 2>/dev/null || true
    fi
    if $BB mount -t iso9660 -o ro "$d" /mnt/iso 2>/dev/null; then
      mounted=1
      echo "mounted=$d" > /run/vilaos/iso.mount
      break 2
    fi
  done
  if [ "$mounted" -eq 0 ]; then
    for n in 0 1 2 3; do
      [ -e "/sys/class/block/sr$n" ] || continue
      $BB mknod "/dev/sr$n" b 11 "$n" 2>/dev/null || true
    done
  fi
  i=$((i + 1))
  $BB sleep 1
done
$BB rm -rf /opt/chromium 2>/dev/null || true
if [ -x /mnt/iso/chromium/chrome ]; then
  if $BB ln -sfn /mnt/iso/chromium /opt/chromium 2>/dev/null; then
    :
  else
    mkdir -p /opt/chromium
    $BB mount --bind /mnt/iso/chromium /opt/chromium 2>/dev/null || true
  fi
elif [ -x /mnt/iso/CHROMIUM/CHROME ]; then
  if $BB ln -sfn /mnt/iso/CHROMIUM /opt/chromium 2>/dev/null; then
    $BB ln -sfn CHROME /opt/chromium/chrome 2>/dev/null || true
  else
    mkdir -p /opt/chromium
    $BB mount --bind /mnt/iso/CHROMIUM /opt/chromium 2>/dev/null || true
  fi
elif [ -x /mnt/iso/chromium/chromium ]; then
  if $BB ln -sfn /mnt/iso/chromium /opt/chromium 2>/dev/null; then
    :
  else
    mkdir -p /opt/chromium
    $BB mount --bind /mnt/iso/chromium /opt/chromium 2>/dev/null || true
  fi
fi
mkdir -p /run/user/0
for f in icudtl.dat v8_context_snapshot.bin snapshot_blob.bin chrome_100_percent.pak chrome_200_percent.pak resources.pak; do
  if [ -f "/opt/chromium-data/$f" ] && [ -f "/mnt/iso/chromium/$f" ]; then
    $BB mount --bind "/opt/chromium-data/$f" "/mnt/iso/chromium/$f" 2>/dev/null || true
  fi
done
if [ -d /opt/chromium-data/locales ] && [ -d /mnt/iso/chromium/locales ]; then
  $BB mount --bind /opt/chromium-data/locales /mnt/iso/chromium/locales 2>/dev/null || true
fi
if [ ! -x /opt/chromium/chrome ] && [ -x /mnt/iso/chromium/chrome ]; then
  export PATH="/mnt/iso/chromium:$PATH"
fi
mkdir -p /dev/dri /run/user/0
$BB chmod 700 /run/user/0 2>/dev/null || true
[ -e /dev/dri/card0 ] || $BB mknod /dev/dri/card0 c 226 0 2>/dev/null || true
[ -e /dev/dri/renderD128 ] || $BB mknod /dev/dri/renderD128 c 226 128 2>/dev/null || true
{
  echo "time=$($BB date 2>/dev/null || echo unknown)"
  echo "mounted=$mounted"
  $BB ls -la /dev/sr0 /dev/hdb /dev/hdc 2>&1 || true
  $BB ls /mnt/iso 2>&1 || true
  $BB ls /mnt/iso/chromium 2>&1 || true
  if [ -L /opt/chromium ] || [ -x /opt/chromium/chrome ]; then
    echo "chromium_ready=1"
    $BB ls -la /opt/chromium/chrome /opt/chromium/lib64/ld-linux-x86-64.so.2 2>&1 || true
    $BB ls -la /tmp/chrome-overlay/icudtl.dat /mnt/iso/chromium/icudtl.dat /opt/chromium-data/icudtl.dat 2>&1 || true
    $BB ls /opt/chromium/lib 2>&1 | $BB head -n 40 || true
  else
    echo "chromium_ready=0"
  fi
  $BB ls -la /dev/dri 2>&1 || true
  test -x /usr/bin/cage && echo "cage=1" || echo "cage=0"
  test -x /usr/bin/weston && echo "weston=1" || echo "weston=0"
  $BB ls /usr/lib/x86_64-linux-gnu/dri 2>&1 | $BB head -n 20 || true
  if [ -x /bin/vilaos-chromium ]; then echo "wrapper=1"; else echo "wrapper=0"; fi
} > /run/vilaos/chromium.status
$BB cp /run/vilaos/chromium.status /run/vilaos/chromium.log 2>/dev/null || true
EOF
chmod 755 "${ROOTFS}/bin/vilaos-chrome-setup"

cat > "${ROOTFS}/init" << "EOF"
#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
BB=/bin/busybox
$BB mount -t proc proc /proc
$BB mount -t sysfs sysfs /sys
$BB mount -t devtmpfs devtmpfs /dev 2>/dev/null || {
  $BB mknod /dev/fb0 c 29 0 2>/dev/null
  $BB mknod /dev/tty0 c 4 0 2>/dev/null
  $BB mknod /dev/ttyS0 c 4 64 2>/dev/null
  mkdir -p /dev/input
}
mkdir -p /tmp /dev/input /apps /data/apps /var/lib/vilaos /packages /drivers/gfx /lib /run/vilaos /ui /opt /mnt/iso
$BB mount -t tmpfs tmpfs /run 2>/dev/null || true
$BB mount -t tmpfs -o size=128m,mode=1777 tmpfs /tmp 2>/dev/null || true
mkdir -p /dev/shm /run/vilaos /run/user/0 /tmp/chromium-data /tmp/chromium-config /tmp/chromium-cache
$BB mount -t tmpfs -o size=64m,mode=1777 tmpfs /dev/shm 2>/dev/null || true
$BB chmod 700 /run/user/0 2>/dev/null || true
[ -e /dev/fb0 ] || $BB mknod /dev/fb0 c 29 0 2>/dev/null
[ -e /dev/tty0 ] || $BB mknod /dev/tty0 c 4 0 2>/dev/null
$BB ifconfig lo 127.0.0.1 netmask 255.0.0.0 up 2>/dev/null || $BB ip link set lo up 2>/dev/null || true
$BB ip addr add 127.0.0.1/8 dev lo 2>/dev/null || true
$BB ip link set lo up 2>/dev/null || true
/bin/vilaos-chrome-setup
export LD_LIBRARY_PATH=/opt/chromium:/opt/chromium/lib:/opt/chromium/lib64:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu:/lib:/drivers/gfx/builtin
export LIBGL_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
export HOME=/tmp
export XDG_RUNTIME_DIR=/run/user/0
export XDG_CONFIG_HOME=/tmp/chromium-config
export XDG_CACHE_HOME=/tmp/chromium-cache
export VILAOS_HEADLESS_UI=1
cd /
exec /bin/vilaos
EOF
chmod 755 "${ROOTFS}/init"
ln -sf /init "${ROOTFS}/sbin/init"

(cd "${ROOTFS}" && find . | cpio -o -H newc 2>/dev/null | gzip -9 > "${WORK}/build/rootfs.cpio.gz")

ISO_ROOT="${WORK}/build/iso"
ISO_OUT="${WORK}/build/vilaos.iso"
CMDLINE="console=tty0 vga=0x311 quiet"
rm -rf "${ISO_ROOT}"
mkdir -p "${ISO_ROOT}/boot/grub" "${ISO_ROOT}/isolinux" "${ISO_ROOT}/chromium"
cp -f "${WORK}/build/bzImage" "${ISO_ROOT}/boot/bzImage"
cp -f "${WORK}/build/rootfs.cpio.gz" "${ISO_ROOT}/boot/rootfs.cpio.gz"
bash scripts/stage_chromium_rootfs.sh "${ROOTFS}" /root/vilaos-work/chromium "${ISO_ROOT}" || true
cat > "${ISO_ROOT}/boot/grub/grub.cfg" << EOF
set timeout=0
set default=0
menuentry "VilaOS" {
  linux /boot/bzImage ${CMDLINE}
  initrd /boot/rootfs.cpio.gz
}
EOF

ISOLINUX_BIN=""
LDLINUX=""
for d in /usr/lib/ISOLINUX /usr/lib/syslinux/modules/bios /usr/share/syslinux; do
  [[ -f "${d}/isolinux.bin" ]] && ISOLINUX_BIN="${d}/isolinux.bin"
  [[ -f "${d}/ldlinux.c32" ]] && LDLINUX="${d}/ldlinux.c32"
done
cp -f "${ISOLINUX_BIN}" "${ISO_ROOT}/isolinux/isolinux.bin"
[[ -n "${LDLINUX}" ]] && cp -f "${LDLINUX}" "${ISO_ROOT}/isolinux/ldlinux.c32"
cat > "${ISO_ROOT}/isolinux/isolinux.cfg" << EOF
DEFAULT vilaos
PROMPT 0
TIMEOUT 0
LABEL vilaos
  KERNEL /boot/bzImage
  APPEND initrd=/boot/rootfs.cpio.gz ${CMDLINE}
EOF
genisoimage -o "${ISO_OUT}" \
  -b isolinux/isolinux.bin -c isolinux/boot.cat \
  -no-emul-boot -boot-load-size 4 -boot-info-table \
  -J -joliet-long -R -r -V VILAOS \
  "${ISO_ROOT}"

mkdir -p "${HOST}/build/packages"
cp -f "${ISO_OUT}" "${WORK}/build/bzImage" "${WORK}/build/rootfs.cpio.gz" "${HOST}/build/"
cp -a "${WORK}/build/packages/." "${HOST}/build/packages/" 2>/dev/null || true
if [[ -f "${WORK}/build/libvilaos_gfx.so" ]]; then
  cp -f "${WORK}/build/libvilaos_gfx.so" "${HOST}/build/"
fi
ls -lh "${HOST}/build/vilaos.iso" "${HOST}/build/bzImage" "${HOST}/build/rootfs.cpio.gz"
echo "ISO ready: ${HOST}/build/vilaos.iso"
