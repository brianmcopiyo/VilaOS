#!/usr/bin/env bash
set -euo pipefail

ROOTFS="${1:?rootfs}"
CACHE="${2:-/root/vilaos-work/chromium}"
ISO_EXTRA="${3:-}"
mkdir -p "${ROOTFS}/opt/chromium" "${ROOTFS}/ui" "${ROOTFS}/tmp"

if [[ -d shell/web ]]; then
  cp -a shell/web/. "${ROOTFS}/ui/"
fi

stage_deps() {
  local dest="$1"
  local bin="$2"
  mkdir -p "${dest}/lib" "${dest}/lib64"
  if [[ -f /lib64/ld-linux-x86-64.so.2 ]]; then
    cp -aL /lib64/ld-linux-x86-64.so.2 "${dest}/lib64/" 2>/dev/null || true
  elif [[ -f /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ]]; then
    cp -aL /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 "${dest}/lib64/" 2>/dev/null || true
  fi
  if command -v apt-get >/dev/null 2>&1; then
    apt-get install -y -qq libnss3 libnspr4 libatk1.0-0 libatk-bridge2.0-0 libcups2 libdrm2 \
      libxkbcommon0 libxcomposite1 libxdamage1 libxfixes3 libxrandr2 libgbm1 libasound2t64 \
      libasound2 libpango-1.0-0 libcairo2 libx11-6 libxext6 libxcb1 libegl1 libgl1-mesa-dri \
      libdbus-1-3 libxcb-dri3-0 libxshmfence1 libxss1 fonts-dejavu-core 2>/dev/null || true
  fi
  copy_so() {
    local so="$1"
    [[ -f "${so}" ]] || return 0
    cp -aL "${so}" "${dest}/lib/" 2>/dev/null || true
    local real
    real="$(readlink -f "${so}" 2>/dev/null || true)"
    if [[ -n "${real}" && -f "${real}" ]]; then
      cp -aL "${real}" "${dest}/lib/" 2>/dev/null || true
    fi
  }
  if command -v ldd >/dev/null 2>&1 && [[ -x "${bin}" ]]; then
    ldd "${bin}" 2>/dev/null | awk '/=>/ {print $3} /^\// {print $1}' | while read -r so; do
      copy_so "${so}"
    done
  fi
  local names=(
    libnss3.so libnss3.so.2 libnssutil3.so libsmime3.so libssl3.so libnspr4.so libplc4.so libplds4.so
    libgbm.so.1 libdrm.so.2 libEGL.so.1 libGL.so.1 libexpat.so.1 libdbus-1.so.3
    libatk-1.0.so.0 libatk-bridge-2.0.so.0 libatspi.so.0 libcups.so.2
    libX11.so.6 libXcomposite.so.1 libXdamage.so.1 libXext.so.6 libXfixes.so.3 libXrandr.so.2
    libxkbcommon.so.0 libxcb.so.1 libxcb-dri3.so.0 libXss.so.1 libasound.so.2
    libpango-1.0.so.0 libpangocairo-1.0.so.0 libcairo.so.2 libgio-2.0.so.0 libglib-2.0.so.0
    libgobject-2.0.so.0 libfontconfig.so.1 libfreetype.so.6 libharfbuzz.so.0
  )
  local d so
  for d in /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu /lib64 /usr/lib; do
    for so in "${names[@]}"; do
      [[ -e "${d}/${so}" ]] || continue
      copy_so "${d}/${so}"
    done
    if [[ -d "${d}/nss" ]]; then
      cp -aL "${d}/nss/." "${dest}/lib/" 2>/dev/null || true
    fi
  done
  if command -v ldd >/dev/null 2>&1; then
    local pass
    for pass in 1 2 3; do
      find "${dest}/lib" -maxdepth 1 -type f -name 'lib*.so*' 2>/dev/null | while read -r lib; do
        ldd "${lib}" 2>/dev/null | awk '/=>/ {print $3}' | while read -r so; do
          copy_so "${so}"
        done
      done
    done
  fi
  if command -v ldd >/dev/null 2>&1 && [[ -x "${bin}" ]]; then
    export LD_LIBRARY_PATH="${dest}/lib:${dest}:${LD_LIBRARY_PATH:-}"
    local pass name found
    for pass in 1 2 3 4 5 6 7 8; do
      local missing
      missing="$(ldd "${bin}" 2>/dev/null | awk '/not found/ {print $1}' || true)"
      [[ -z "${missing}" ]] && break
      while read -r name; do
        [[ -z "${name}" ]] && continue
        found="$(find /lib /usr/lib /lib64 -name "${name}" 2>/dev/null | head -n 1 || true)"
        if [[ -n "${found}" ]]; then
          copy_so "${found}"
        fi
      done <<< "${missing}"
    done
  fi
  ls "${dest}/lib"/libnss3.so* 2>/dev/null || echo "WARN: libnss3 missing in ${dest}/lib"
  ls "${dest}/lib"/libasound.so* 2>/dev/null || echo "WARN: libasound missing in ${dest}/lib"
}

if [[ -n "${ISO_EXTRA}" ]]; then
  mkdir -p "${ISO_EXTRA}/chromium"
  if [[ -x "${CACHE}/chrome" ]]; then
    cp -a "${CACHE}/." "${ISO_EXTRA}/chromium/"
    ln -sfn chrome "${ISO_EXTRA}/chromium/chromium"
    stage_deps "${ISO_EXTRA}/chromium" "${ISO_EXTRA}/chromium/chrome"
    echo "staged chromium onto ISO tree ${ISO_EXTRA}/chromium"
  elif [[ -x "${CACHE}/chromium" ]]; then
    cp -a "${CACHE}/." "${ISO_EXTRA}/chromium/"
    stage_deps "${ISO_EXTRA}/chromium" "${ISO_EXTRA}/chromium/chromium"
  else
    echo "no chromium cache at ${CACHE}"
  fi
else
  if [[ -x "${CACHE}/chrome" ]]; then
    cp -a "${CACHE}/." "${ROOTFS}/opt/chromium/"
    ln -sfn chrome "${ROOTFS}/opt/chromium/chromium"
    stage_deps "${ROOTFS}/opt/chromium" "${ROOTFS}/opt/chromium/chrome"
  fi
fi
