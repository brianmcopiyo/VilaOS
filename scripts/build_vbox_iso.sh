#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"
ISO_ROOT="${BUILD}/iso"
ISO_OUT="${BUILD}/vilaos.iso"
CMDLINE='console=tty0 vga=0x311 quiet'

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing required command: $1" >&2
    exit 1
  fi
}

echo "==> Ensuring kernel + initramfs"
if [[ -x "${ROOT}/scripts/rebuild_iso_quick.sh" ]] && [[ -d /root/vilaos-work/src ]]; then
  bash "${ROOT}/scripts/rebuild_iso_quick.sh"
else
  bash "${ROOT}/scripts/build_qemu_image.sh"
  if [[ -d /root/vilaos-work/src/build/rootfs ]]; then
    bash "${ROOT}/scripts/stage_wayland_rootfs.sh" /root/vilaos-work/src/build/rootfs || true
  fi
fi

test -f "${BUILD}/bzImage"
test -f "${BUILD}/rootfs.cpio.gz"

BZ_SZ=$(stat -c%s "${BUILD}/bzImage")
RD_SZ=$(stat -c%s "${BUILD}/rootfs.cpio.gz")
echo "bzImage=${BZ_SZ} rootfs.cpio.gz=${RD_SZ}"
if [[ "${BZ_SZ}" -gt 4500000 ]]; then
  echo "warning: bzImage may be too large for the 32 MB minimum RAM target (${BZ_SZ} bytes)" >&2
fi

echo "==> Staging ISO tree"
rm -rf "${ISO_ROOT}"
mkdir -p "${ISO_ROOT}/boot/grub"
cp -f "${BUILD}/bzImage" "${ISO_ROOT}/boot/bzImage"
cp -f "${BUILD}/rootfs.cpio.gz" "${ISO_ROOT}/boot/rootfs.cpio.gz"

cat > "${ISO_ROOT}/boot/grub/grub.cfg" << EOF
set timeout=0
set default=0
menuentry "VilaOS" {
  linux /boot/bzImage ${CMDLINE}
  initrd /boot/rootfs.cpio.gz
}
EOF

make_iso_isolinux() {
  need_cmd genisoimage
  local isolinux_bin=""
  local ldlinux=""
  for d in /usr/lib/ISOLINUX /usr/lib/syslinux/modules/bios /usr/share/syslinux; do
    if [[ -f "${d}/isolinux.bin" ]]; then
      isolinux_bin="${d}/isolinux.bin"
    fi
    if [[ -f "${d}/ldlinux.c32" ]]; then
      ldlinux="${d}/ldlinux.c32"
    fi
  done
  if [[ -z "${isolinux_bin}" ]]; then
    return 1
  fi
  mkdir -p "${ISO_ROOT}/isolinux"
  cp -f "${isolinux_bin}" "${ISO_ROOT}/isolinux/isolinux.bin"
  if [[ -n "${ldlinux}" ]]; then
    cp -f "${ldlinux}" "${ISO_ROOT}/isolinux/ldlinux.c32"
  fi
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
  if command -v isohybrid >/dev/null 2>&1; then
    isohybrid "${ISO_OUT}" || true
  fi
  return 0
}

make_iso_grub() {
  need_cmd grub-mkrescue
  need_cmd xorriso
  grub-mkrescue \
    --install-modules="linux normal configfile biosdisk iso9660 part_msdos search search_fs_uuid search_label fat" \
    --fonts="" \
    --locales="" \
    --themes="" \
    -o "${ISO_OUT}" \
    "${ISO_ROOT}"
}

echo "==> Creating hybrid ISO"
if make_iso_isolinux; then
  echo "ISO bootloader: isolinux"
elif command -v grub-mkrescue >/dev/null 2>&1 && command -v xorriso >/dev/null 2>&1; then
  make_iso_grub
  echo "ISO bootloader: grub-mkrescue (minimal modules)"
else
  echo "Need isolinux+genisoimage or grub-mkrescue+xorriso" >&2
  exit 1
fi

test -f "${ISO_OUT}"
ls -lh "${BUILD}/bzImage" "${BUILD}/rootfs.cpio.gz" "${ISO_OUT}"
echo
echo "ISO ready: ${ISO_OUT}"
echo "Next (Windows PowerShell):"
echo "  .\\scripts\\run_vbox.ps1"
