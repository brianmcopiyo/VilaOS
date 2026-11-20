#!/usr/bin/env bash
set -euo pipefail
HOST="/mnt/c/Users/HP/Desktop/OS"
WORK="/root/vilaos-work/src"
ROOTFS="${WORK}/build/rootfs"
find "${HOST}/scripts" -name '*.sh' -exec sed -i 's/\r$//' {} +
rsync -a "${HOST}/scripts/vilaos-chromium.sh" "${HOST}/scripts/stage_wayland_rootfs.sh" "${HOST}/scripts/restage_chromium_iso.sh" "${WORK}/scripts/"
install -m 755 "${HOST}/scripts/vilaos-chromium.sh" "${ROOTFS}/bin/vilaos-chromium"
bash "${WORK}/scripts/stage_wayland_rootfs.sh" "${ROOTFS}"
cp -f /root/vilaos-work/chromium/icudtl.dat "${ROOTFS}/opt/chromium-data/icudtl.dat"
ls -la "${ROOTFS}/opt/chromium-data/icudtl.dat"
ls "${ROOTFS}/usr/lib/x86_64-linux-gnu"/libwlroots* 2>/dev/null || ls "${ROOTFS}/lib/x86_64-linux-gnu"/libwlroots* 2>/dev/null || echo "wlroots missing"
cd "${WORK}"
bash scripts/restage_chromium_iso.sh
