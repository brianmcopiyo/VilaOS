#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ROOT}/build/packages"
SDK="${ROOT}/sdk/vilaos-sdk"
mkdir -p "${OUT}"
chmod +x "${SDK}" 2>/dev/null || true

for app in launcher dialer messages contacts notes calculator clock files browser settings music; do
  dir="${ROOT}/apps/${app}"
  [[ -d "${dir}" ]] || continue
  bash "${SDK}" validate "${dir}"
  bash "${SDK}" pack "${dir}" -o "${OUT}/${app}.vpkg"
done

ls -lh "${OUT}"
echo "Packages ready in ${OUT}"
