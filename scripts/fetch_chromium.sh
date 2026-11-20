#!/usr/bin/env bash
set -euo pipefail

DEST="${1:-/root/vilaos-work/chromium}"
mkdir -p "${DEST}"

is_real_chrome() {
  local p="$1"
  [[ -x "${p}" ]] || return 1
  local sz
  sz="$(stat -c%s "${p}" 2>/dev/null || echo 0)"
  [[ "${sz}" -gt 1000000 ]] || return 1
  file -b "${p}" 2>/dev/null | grep -qiE 'ELF|executable' || return 1
  return 0
}

if is_real_chrome "${DEST}/chrome" || is_real_chrome "${DEST}/chromium"; then
  echo "chromium cache ready: ${DEST}"
  ls -lh "${DEST}/chrome" "${DEST}/chromium" 2>/dev/null || true
  exit 0
fi

TMP="${DEST}/.tmp"
rm -rf "${TMP}"
mkdir -p "${TMP}"

URLS=(
  "https://commondatastorage.googleapis.com/chromium-browser-snapshots/Linux_x64/1313161/chrome-linux.zip"
  "https://storage.googleapis.com/chromium-browser-snapshots/Linux_x64/1313161/chrome-linux.zip"
)

ok=0
for url in "${URLS[@]}"; do
  echo "fetch ${url}"
  if command -v curl >/dev/null 2>&1; then
    curl -L --fail --retry 3 --retry-delay 2 -o "${TMP}/chrome.zip" "${url}" || continue
  else
    wget -O "${TMP}/chrome.zip" "${url}" || continue
  fi
  command -v unzip >/dev/null 2>&1 || apt-get install -y -qq unzip >/dev/null 2>&1 || true
  unzip -q "${TMP}/chrome.zip" -d "${TMP}" || continue
  if is_real_chrome "${TMP}/chrome-linux/chrome"; then
    find "${DEST}" -mindepth 1 -maxdepth 1 ! -name '.tmp' -exec rm -rf {} +
    cp -a "${TMP}/chrome-linux/." "${DEST}/"
    chmod 755 "${DEST}/chrome"
    ln -sfn chrome "${DEST}/chromium"
    ok=1
    break
  fi
done

rm -rf "${TMP}"
if [[ "${ok}" -eq 1 ]]; then
  echo "chromium snapshot ready: ${DEST}"
  ls -lh "${DEST}/chrome"
  exit 0
fi

echo "chromium unavailable; shell will keep native fallback" >&2
exit 0
