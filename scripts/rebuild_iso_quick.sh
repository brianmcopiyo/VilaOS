#!/usr/bin/env bash
set -euo pipefail

HOST="/mnt/c/Users/HP/Desktop/OS"
WORK="/root/vilaos-work/src"
OLD="/root/phoneos-work/src"

find "${HOST}/src" "${HOST}/include" "${HOST}/assets" "${HOST}/apps" "${HOST}/sdk" "${HOST}/scripts" -type f \( -name "*.c" -o -name "*.h" -o -name "*.xml" -o -name "*.sh" -o -name "*.kt" -o -name "*.kbc" -o -name "Makefile" -o -name "vilaos-sdk" -o -name "ktc" \) -exec sed -i "s/\r$//" {} + 2>/dev/null || true

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
make clean
make musl
strip --strip-unneeded vilaos-musl vilaos-pkg-musl vilaos-musicd-musl
bash scripts/pack_apps.sh
bash scripts/pack_gfx_driver.sh

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
mkdir -p "${ROOTFS}"/{bin,sbin,dev,proc,sys,tmp,lib,run/vilaos,assets/screens,assets/values,assets/components,assets/ui,assets/fonts,fonts,res/fonts,sdk/ui,apps,data/apps,var/lib/vilaos,packages,drivers/gfx/builtin}
cp -f "${BB}" "${ROOTFS}/bin/busybox"
for a in sh ls cat mount umount mkdir mknod chmod chown sleep ps free dmesg tar; do
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
cp -a apps/. "${ROOTFS}/apps/"
cp -a build/packages/. "${ROOTFS}/packages/" 2>/dev/null || true

cat > "${ROOTFS}/init" << "EOF"
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev 2>/dev/null || {
  mknod /dev/fb0 c 29 0 2>/dev/null
  mknod /dev/tty0 c 4 0 2>/dev/null
  mknod /dev/ttyS0 c 4 64 2>/dev/null
  mkdir -p /dev/input
}
mkdir -p /tmp /dev/input /apps /data/apps /var/lib/vilaos /packages /drivers/gfx /lib /run/vilaos
mount -t tmpfs tmpfs /run 2>/dev/null || true
mkdir -p /run/vilaos
[ -e /dev/fb0 ] || mknod /dev/fb0 c 29 0 2>/dev/null
[ -e /dev/tty0 ] || mknod /dev/tty0 c 4 0 2>/dev/null
export LD_LIBRARY_PATH=/lib:/drivers/gfx/builtin
cd /
export HOME=/
while true; do
  /bin/vilaos
  sleep 1
done
EOF
chmod 755 "${ROOTFS}/init"
ln -sf /init "${ROOTFS}/sbin/init"

(cd "${ROOTFS}" && find . | cpio -o -H newc 2>/dev/null | gzip -9 > "${WORK}/build/rootfs.cpio.gz")

ISO_ROOT="${WORK}/build/iso"
ISO_OUT="${WORK}/build/vilaos.iso"
CMDLINE="console=tty0 vga=0x311 quiet"
rm -rf "${ISO_ROOT}"
mkdir -p "${ISO_ROOT}/boot/grub" "${ISO_ROOT}/isolinux"
cp -f "${WORK}/build/bzImage" "${ISO_ROOT}/boot/bzImage"
cp -f "${WORK}/build/rootfs.cpio.gz" "${ISO_ROOT}/boot/rootfs.cpio.gz"
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
  -J -R -V VILAOS \
  "${ISO_ROOT}"

mkdir -p "${HOST}/build/packages"
cp -f "${ISO_OUT}" "${WORK}/build/bzImage" "${WORK}/build/rootfs.cpio.gz" "${HOST}/build/"
cp -a "${WORK}/build/packages/." "${HOST}/build/packages/" 2>/dev/null || true
if [[ -f "${WORK}/build/libvilaos_gfx.so" ]]; then
  cp -f "${WORK}/build/libvilaos_gfx.so" "${HOST}/build/"
fi
ls -lh "${HOST}/build/vilaos.iso" "${HOST}/build/bzImage" "${HOST}/build/rootfs.cpio.gz"
echo "ISO ready: ${HOST}/build/vilaos.iso"
