#!/usr/bin/env bash
set -euo pipefail

HOST="/mnt/c/Users/HP/Desktop/OS"
WORK="/root/vilaos-work/src"
OLD="/root/phoneos-work/src"

find "${HOST}/src" "${HOST}/include" "${HOST}/assets" "${HOST}/scripts" -type f \( -name "*.c" -o -name "*.h" -o -name "*.xml" -o -name "*.sh" -o -name "Makefile" \) -exec sed -i "s/\r$//" {} +

mkdir -p "${WORK}/build"
rsync -a --delete \
  --exclude build \
  --exclude ".git" \
  --exclude "*.png" \
  --exclude "vilaos" \
  --exclude "vilaos-*" \
  --exclude "phoneos" \
  --exclude "phoneos-*" \
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
ln -sfn /usr/include/x86_64-linux-gnu/asm /usr/include/x86_64-linux-musl/asm

cd "${WORK}"
make clean
make musl
strip --strip-unneeded vilaos-musl

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

ROOTFS="${WORK}/build/rootfs"
rm -rf "${ROOTFS}"
mkdir -p "${ROOTFS}"/{bin,sbin,dev,proc,sys,tmp,assets/screens}
cp -f "${BB}" "${ROOTFS}/bin/busybox"
for a in sh ls cat mount umount mkdir mknod chmod chown sleep ps free dmesg; do
  ln -sf busybox "${ROOTFS}/bin/${a}"
done
cp -f vilaos-musl "${ROOTFS}/bin/vilaos"
chmod 755 "${ROOTFS}/bin/vilaos"
cp -a assets/screens/. "${ROOTFS}/assets/screens/"

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
mkdir -p /tmp /dev/input
[ -e /dev/fb0 ] || mknod /dev/fb0 c 29 0 2>/dev/null
[ -e /dev/tty0 ] || mknod /dev/tty0 c 4 0 2>/dev/null
cd /
export HOME=/
exec /bin/vilaos
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

cp -f "${ISO_OUT}" "${WORK}/build/bzImage" "${WORK}/build/rootfs.cpio.gz" "${HOST}/build/"
ls -lh "${HOST}/build/vilaos.iso" "${HOST}/build/bzImage" "${HOST}/build/rootfs.cpio.gz"
echo "ISO ready: ${HOST}/build/vilaos.iso"
