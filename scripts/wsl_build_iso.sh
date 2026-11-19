#!/usr/bin/env bash
set -euo pipefail

HOST_ROOT="/mnt/c/Users/HP/Desktop/OS"
WORK="/root/vilaos-work"
SRC="${WORK}/src"

mkdir -p "${SRC}/build" "${HOST_ROOT}/build"

echo "==> Syncing sources to ${SRC}"
rsync -a --delete \
  --exclude build \
  --exclude '.git' \
  --exclude '*.png' \
  --exclude 'vilaos' \
  --exclude 'vilaos-*' \
  --exclude 'phoneos' \
  --exclude 'phoneos-*' \
  "${HOST_ROOT}/" "${SRC}/"

mkdir -p "${SRC}/build"
for f in busybox-1.36.1.tar.bz2 linux-6.6.22.tar.xz; do
  if [[ -f "${HOST_ROOT}/build/${f}" && ! -f "${SRC}/build/${f}" ]]; then
    echo "==> Copying ${f}"
    cp -f "${HOST_ROOT}/build/${f}" "${SRC}/build/${f}"
  fi
done

if [[ -d /root/phoneos-work/src/build ]]; then
  for f in bzImage busybox busybox-1.36.1.tar.bz2 linux-6.6.22.tar.xz; do
    if [[ -e "/root/phoneos-work/src/build/${f}" && ! -e "${SRC}/build/${f}" ]]; then
      cp -a "/root/phoneos-work/src/build/${f}" "${SRC}/build/${f}"
    fi
  done
  if [[ -d /root/phoneos-work/src/build/linux-6.6.22 && ! -d "${SRC}/build/linux-6.6.22" ]]; then
    cp -a /root/phoneos-work/src/build/linux-6.6.22 "${SRC}/build/"
  fi
  if [[ -d /root/phoneos-work/src/build/busybox-1.36.1 && ! -d "${SRC}/build/busybox-1.36.1" ]]; then
    cp -a /root/phoneos-work/src/build/busybox-1.36.1 "${SRC}/build/"
  fi
fi

ln -sfn /usr/include/linux /usr/include/x86_64-linux-musl/linux 2>/dev/null || true
ln -sfn /usr/include/asm-generic /usr/include/x86_64-linux-musl/asm-generic 2>/dev/null || true
ln -sfn /usr/include/x86_64-linux-gnu/asm /usr/include/x86_64-linux-musl/asm 2>/dev/null || true

cd "${SRC}"
bash scripts/build_vbox_iso.sh

cp -f "${SRC}/build/bzImage" "${HOST_ROOT}/build/bzImage"
cp -f "${SRC}/build/rootfs.cpio.gz" "${HOST_ROOT}/build/rootfs.cpio.gz"
cp -f "${SRC}/build/vilaos.iso" "${HOST_ROOT}/build/vilaos.iso"
ls -lh "${HOST_ROOT}/build/bzImage" "${HOST_ROOT}/build/rootfs.cpio.gz" "${HOST_ROOT}/build/vilaos.iso"
echo "ISO ready: ${HOST_ROOT}/build/vilaos.iso"
