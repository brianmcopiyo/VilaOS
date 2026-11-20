#!/usr/bin/env bash
set -euo pipefail

ROOTFS="${1:?rootfs}"
mkdir -p "${ROOTFS}/usr/bin" "${ROOTFS}/usr/lib" "${ROOTFS}/usr/lib/x86_64-linux-gnu" \
  "${ROOTFS}/lib" "${ROOTFS}/lib/x86_64-linux-gnu" "${ROOTFS}/lib64" "${ROOTFS}/usr/share" \
  "${ROOTFS}/usr/lib/x86_64-linux-gnu/dri" "${ROOTFS}/etc" "${ROOTFS}/bin"

if command -v apt-get >/dev/null 2>&1; then
  apt-get install -y -qq cage weston seatd libwayland-client0 libwayland-server0 \
    libwayland-cursor0 libwayland-egl1 libxkbcommon0 libinput10 libudev1 \
    libgbm1 libegl1 libgles2 libgl1-mesa-dri mesa-vulkan-drivers libglapi-mesa \
    libdrm2 libpixman-1-0 libseat1 2>/dev/null || \
  apt-get install -y -qq weston seatd libwayland-client0 libwayland-server0 \
    libwayland-cursor0 libwayland-egl1 libxkbcommon0 libinput10 \
    libgbm1 libegl1 libgles2 libgl1-mesa-dri libdrm2 libpixman-1-0 2>/dev/null || true
fi

copy_so_tree() {
  local so="$1"
  [[ -e "${so}" ]] || return 0
  local dest_dir="${ROOTFS}$(dirname "${so}")"
  mkdir -p "${dest_dir}"
  cp -aL "${so}" "${dest_dir}/" 2>/dev/null || true
  local real
  real="$(readlink -f "${so}" 2>/dev/null || true)"
  if [[ -n "${real}" && -f "${real}" ]]; then
    cp -aL "${real}" "${dest_dir}/" 2>/dev/null || true
  fi
}

if [[ -f /lib64/ld-linux-x86-64.so.2 ]]; then
  cp -aL /lib64/ld-linux-x86-64.so.2 "${ROOTFS}/lib64/" 2>/dev/null || true
elif [[ -f /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ]]; then
  cp -aL /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 "${ROOTFS}/lib64/" 2>/dev/null || true
  cp -aL /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 "${ROOTFS}/lib/x86_64-linux-gnu/" 2>/dev/null || true
fi

for so in libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 librt.so.1 libresolv.so.2 \
  libgcc_s.so.1 libstdc++.so.6; do
  for d in /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu /lib64; do
    [[ -e "${d}/${so}" ]] || continue
    copy_so_tree "${d}/${so}"
  done
done

copy_bin() {
  local b="$1"
  local src=""
  for d in /usr/bin /bin /usr/sbin /sbin; do
    if [[ -x "${d}/${b}" ]]; then
      src="${d}/${b}"
      break
    fi
  done
  [[ -n "${src}" ]] || return 0
  cp -aL "${src}" "${ROOTFS}/usr/bin/${b}"
  chmod 755 "${ROOTFS}/usr/bin/${b}"
  ln -sfn "../usr/bin/${b}" "${ROOTFS}/bin/${b}" 2>/dev/null || true
}

stage_ldd() {
  local bin="$1"
  [[ -x "${bin}" ]] || return 0
  ldd "${bin}" 2>/dev/null | awk '/=>/ {print $3} /^\// {print $1}' | while read -r so; do
    copy_so_tree "${so}"
  done
}

for b in cage weston seatd; do
  copy_bin "${b}"
done

for b in cage weston seatd; do
  if [[ -x "${ROOTFS}/usr/bin/${b}" ]]; then
    stage_ldd "${ROOTFS}/usr/bin/${b}"
  fi
done

for so in \
  /usr/lib/x86_64-linux-gnu/libwayland-client.so.0 \
  /usr/lib/x86_64-linux-gnu/libwayland-server.so.0 \
  /usr/lib/x86_64-linux-gnu/libwayland-cursor.so.0 \
  /usr/lib/x86_64-linux-gnu/libwayland-egl.so.1 \
  /usr/lib/x86_64-linux-gnu/libxkbcommon.so.0 \
  /usr/lib/x86_64-linux-gnu/libinput.so.10 \
  /usr/lib/x86_64-linux-gnu/libudev.so.1 \
  /usr/lib/x86_64-linux-gnu/libgbm.so.1 \
  /usr/lib/x86_64-linux-gnu/libEGL.so.1 \
  /usr/lib/x86_64-linux-gnu/libGLESv2.so.2 \
  /usr/lib/x86_64-linux-gnu/libGL.so.1 \
  /usr/lib/x86_64-linux-gnu/libdrm.so.2 \
  /usr/lib/x86_64-linux-gnu/libseat.so.1 \
  /usr/lib/x86_64-linux-gnu/libpixman-1.so.0 \
  /usr/lib/x86_64-linux-gnu/libGLdispatch.so.0 \
  /usr/lib/x86_64-linux-gnu/libGLX.so.0; do
  copy_so_tree "${so}"
done

mkdir -p "${ROOTFS}/usr/lib/x86_64-linux-gnu/dri"
rm -rf "${ROOTFS}/usr/lib/x86_64-linux-gnu/dri/"*
if [[ -d /usr/lib/x86_64-linux-gnu/dri ]]; then
  for d in vmwgfx_dri.so kms_swrast_dri.so swrast_dri.so virtio_gpu_dri.so; do
    [[ -f "/usr/lib/x86_64-linux-gnu/dri/${d}" ]] || continue
    cp -aL "/usr/lib/x86_64-linux-gnu/dri/${d}" "${ROOTFS}/usr/lib/x86_64-linux-gnu/dri/" 2>/dev/null || true
  done
fi
if command -v ldd >/dev/null 2>&1; then
  for dri in "${ROOTFS}/usr/lib/x86_64-linux-gnu/dri/"*_dri.so; do
    [[ -f "${dri}" ]] || continue
    stage_ldd "${dri}"
  done
fi
for so in /usr/lib/x86_64-linux-gnu/libwlroots*.so* /lib/x86_64-linux-gnu/libwlroots*.so* \
  /usr/lib/x86_64-linux-gnu/libliftoff.so* /lib/x86_64-linux-gnu/libliftoff.so* \
  /usr/lib/x86_64-linux-gnu/libdisplay-info.so* /lib/x86_64-linux-gnu/libdisplay-info.so* \
  /usr/lib/x86_64-linux-gnu/libeis.so* /lib/x86_64-linux-gnu/libevdev.so* \
  /usr/lib/x86_64-linux-gnu/libvulkan.so.1 /lib/x86_64-linux-gnu/libvulkan.so.1 \
  /usr/lib/x86_64-linux-gnu/libinput.so.10 /lib/x86_64-linux-gnu/libinput.so.10 \
  /usr/lib/x86_64-linux-gnu/libseat.so.1 /lib/x86_64-linux-gnu/libseat.so.1 \
  /usr/lib/x86_64-linux-gnu/libudev.so.1 /lib/x86_64-linux-gnu/libudev.so.1 \
  /usr/lib/x86_64-linux-gnu/libffi.so.8 /lib/x86_64-linux-gnu/libffi.so.8 \
  /usr/lib/x86_64-linux-gnu/libxcb*.so* /lib/x86_64-linux-gnu/libxcb*.so* \
  /usr/lib/x86_64-linux-gnu/libcap.so.2 /lib/x86_64-linux-gnu/libcap.so.2 \
  /usr/lib/x86_64-linux-gnu/libsystemd.so.0 /lib/x86_64-linux-gnu/libsystemd.so.0 \
  /usr/lib/x86_64-linux-gnu/libmtdev.so.1 /lib/x86_64-linux-gnu/libmtdev.so.1 \
  /usr/lib/x86_64-linux-gnu/libwacom.so.9 /lib/x86_64-linux-gnu/libwacom.so.9 \
  /usr/lib/x86_64-linux-gnu/libXau.so.6 /lib/x86_64-linux-gnu/libXau.so.6; do
  copy_so_tree "${so}"
done
if [[ -x /usr/bin/cage ]]; then
  stage_ldd /usr/bin/cage
fi

if [[ -d /usr/lib/x86_64-linux-gnu/weston ]]; then
  mkdir -p "${ROOTFS}/usr/lib/x86_64-linux-gnu/weston"
  cp -a /usr/lib/x86_64-linux-gnu/weston/. "${ROOTFS}/usr/lib/x86_64-linux-gnu/weston/" 2>/dev/null || true
fi
if [[ -d /usr/share/weston ]]; then
  mkdir -p "${ROOTFS}/usr/share/weston"
  cp -a /usr/share/weston/. "${ROOTFS}/usr/share/weston/" 2>/dev/null || true
fi

echo "wayland stage: cage=$(test -x "${ROOTFS}/usr/bin/cage" && echo yes || echo no) weston=$(test -x "${ROOTFS}/usr/bin/weston" && echo yes || echo no)"
ls "${ROOTFS}/usr/lib/x86_64-linux-gnu/dri" 2>/dev/null | head -n 20 || true
