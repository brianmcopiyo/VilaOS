#!/usr/bin/env bash
set -euo pipefail
HOST="/mnt/c/Users/HP/Desktop/OS"
WORK="/root/vilaos-work/src"
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
mkdir -p "${HOST}/build"
cp -f "${ISO_OUT}" "${WORK}/build/bzImage" "${WORK}/build/rootfs.cpio.gz" "${HOST}/build/"
ls -lh "${HOST}/build/vilaos.iso" "${HOST}/build/bzImage"
gunzip -c "${WORK}/build/rootfs.cpio.gz" | cpio -t 2>/dev/null | grep -E 'boot.xml|power.xml|shutdown.xml' || true
echo "ISO repacked"
