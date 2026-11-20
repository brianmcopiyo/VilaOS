#!/usr/bin/env bash
set -euo pipefail

HOST="/mnt/c/Users/HP/Desktop/OS"
WORK="/root/vilaos-work/src"
CACHE="/root/vilaos-work/chromium"
ISO_ROOT="${WORK}/build/iso"
ISO_OUT="${WORK}/build/vilaos.iso"

cd "${HOST}"
find scripts -name '*.sh' -exec sed -i 's/\r$//' {} + 2>/dev/null || true
rsync -a scripts/stage_chromium_rootfs.sh scripts/vilaos-chromium.sh "${WORK}/scripts/"
rsync -a shell/web/ "${WORK}/shell/web/" 2>/dev/null || true

cd "${WORK}"
if command -v apt-get >/dev/null 2>&1; then
  apt-get install -y -qq libnss3 libnspr4 libasound2t64 libasound2 libatk1.0-0 libatk-bridge2.0-0 \
    libcups2 libdrm2 libgbm1 libxkbcommon0 libxcomposite1 libxdamage1 libxfixes3 libxrandr2 \
    libpango-1.0-0 libcairo2 libx11-6 libxext6 libxcb1 libegl1 libdbus-1-3 libxss1 \
    libxcb-dri3-0 libxshmfence1 fonts-dejavu-core 2>/dev/null || true
fi

ROOTFS="${WORK}/build/rootfs"
mkdir -p "${ROOTFS}/bin" "${ROOTFS}/ui" "${ROOTFS}/etc" "${ROOTFS}/root"
install -m 755 scripts/vilaos-chromium.sh "${ROOTFS}/bin/vilaos-chromium"
printf 'root:x:0:0:root:/root:/bin/sh\n' > "${ROOTFS}/etc/passwd"
printf 'root:x:0:\nvideo:x:44:root\n' > "${ROOTFS}/etc/group"
printf 'root::0:0:99999:7:::\n' > "${ROOTFS}/etc/shadow"
chmod 644 "${ROOTFS}/etc/passwd" "${ROOTFS}/etc/group"
chmod 600 "${ROOTFS}/etc/shadow"
if [[ -d shell/web ]]; then
  cp -a shell/web/. "${ROOTFS}/ui/"
fi

bash scripts/stage_chromium_rootfs.sh "${ROOTFS}" "${CACHE}" "${ISO_ROOT}"

CHROME="${ISO_ROOT}/chromium/chrome"
LIBDIR="${ISO_ROOT}/chromium/lib"
mkdir -p "${LIBDIR}"
if [[ -x "${CHROME}" ]]; then
  export LD_LIBRARY_PATH="${LIBDIR}:${ISO_ROOT}/chromium:${LD_LIBRARY_PATH:-}"
  for pass in 1 2 3 4 5 6; do
    missing="$(ldd "${CHROME}" 2>/dev/null | awk '/not found/ {print $1}')"
    [[ -z "${missing}" ]] && break
    echo "pass ${pass} missing: ${missing}"
    while read -r name; do
      [[ -z "${name}" ]] && continue
      found="$(find /lib /usr/lib /lib64 -name "${name}" 2>/dev/null | head -n 1 || true)"
      if [[ -n "${found}" ]]; then
        cp -aL "${found}" "${LIBDIR}/"
        echo "copied ${found}"
      else
        echo "NOT FOUND ON HOST: ${name}"
      fi
    done <<< "${missing}"
  done
  echo "final ldd:"
  ldd "${CHROME}" 2>/dev/null | head -n 80 || true
fi

if [[ -d "${ROOTFS}/ui" ]]; then
  mkdir -p "${ISO_ROOT}/ui"
  cp -a "${ROOTFS}/ui/." "${ISO_ROOT}/ui/" 2>/dev/null || true
fi

(cd "${ROOTFS}" && find . | cpio -o -H newc 2>/dev/null | gzip -9 > "${WORK}/build/rootfs.cpio.gz")
cp -f "${WORK}/build/rootfs.cpio.gz" "${ISO_ROOT}/boot/rootfs.cpio.gz"
cp -f "${WORK}/build/bzImage" "${ISO_ROOT}/boot/bzImage" 2>/dev/null || true

ISOLINUX_BIN=""
LDLINUX=""
for d in /usr/lib/ISOLINUX /usr/lib/syslinux/modules/bios /usr/share/syslinux; do
  [[ -f "${d}/isolinux.bin" ]] && ISOLINUX_BIN="${d}/isolinux.bin"
  [[ -f "${d}/ldlinux.c32" ]] && LDLINUX="${d}/ldlinux.c32"
done
cp -f "${ISOLINUX_BIN}" "${ISO_ROOT}/isolinux/isolinux.bin"
[[ -n "${LDLINUX}" ]] && cp -f "${LDLINUX}" "${ISO_ROOT}/isolinux/ldlinux.c32"

genisoimage -o "${ISO_OUT}" -b isolinux/isolinux.bin -c isolinux/boot.cat \
  -no-emul-boot -boot-load-size 4 -boot-info-table -J -R -V VILAOS "${ISO_ROOT}"
cp -f "${ISO_OUT}" "${HOST}/build/vilaos.iso"
cp -f "${WORK}/build/rootfs.cpio.gz" "${HOST}/build/rootfs.cpio.gz"
ls -lh "${HOST}/build/vilaos.iso"
echo "ISO ready: ${HOST}/build/vilaos.iso"
