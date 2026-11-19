#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${ROOT}/build/packages"
STAGING="${OUT_DIR}/vilaos-gfx-staging"
ID="builtin"
VER="1.0.0"
ABI="1"
SO_SRC="${ROOT}/build/libvilaos_gfx.so"
if [[ ! -f "${SO_SRC}" ]]; then
  SO_SRC="${ROOT}/libvilaos_gfx.so"
fi

rm -rf "${STAGING}"
mkdir -p "${STAGING}/${ID}" "${OUT_DIR}"

cat > "${STAGING}/${ID}/manifest.xml" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<driver>
  <id>${ID}</id>
  <version>${VER}</version>
  <abi>${ABI}</abi>
  <entry>libvilaos_gfx.so</entry>
  <name>vilaos-gfx-builtin</name>
</driver>
EOF

if [[ -f "${SO_SRC}" ]]; then
  cp -f "${SO_SRC}" "${STAGING}/${ID}/libvilaos_gfx.so"
fi

echo "${ID}" > "${STAGING}/active"

tar -C "${STAGING}" -czf "${OUT_DIR}/vilaos-gfx.vdrv" .
echo "packed ${OUT_DIR}/vilaos-gfx.vdrv"
