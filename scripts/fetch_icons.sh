#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "${ROOT}/icons/lucide" "${ROOT}/icons/tabler" "${ROOT}/icons/phosphor" "${ROOT}/third_party" "${ROOT}/tmp_icons"
cd "${ROOT}/tmp_icons"

curl -fsSL -o "${ROOT}/third_party/nanosvg.h" https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvg.h
curl -fsSL -o "${ROOT}/third_party/nanosvgrast.h" https://raw.githubusercontent.com/memononen/nanosvg/master/src/nanosvgrast.h

curl -fsSL -o lucide.tgz https://codeload.github.com/lucide-icons/lucide/tar.gz/refs/heads/main
tar -xzf lucide.tgz
LUC=$(find . -maxdepth 1 -type d -name 'lucide-*' | head -1)
if [[ -d "${LUC}/icons" ]]; then
  find "${LUC}/icons" -type f -name '*.svg' -exec cp -f {} "${ROOT}/icons/lucide/" \;
fi
if [[ -f "${LUC}/LICENSE" ]]; then
  cp -f "${LUC}/LICENSE" "${ROOT}/icons/lucide/LICENSE"
elif [[ -f "${LUC}/LICENSE.md" ]]; then
  cp -f "${LUC}/LICENSE.md" "${ROOT}/icons/lucide/LICENSE"
fi

curl -fsSL -o tabler.tgz https://codeload.github.com/tabler/tabler-icons/tar.gz/refs/heads/main
tar -xzf tabler.tgz
TAB=$(find . -maxdepth 1 -type d -name 'tabler-icons-*' | head -1)
if [[ -d "${TAB}/icons/outline" ]]; then
  find "${TAB}/icons/outline" -type f -name '*.svg' -exec cp -f {} "${ROOT}/icons/tabler/" \;
elif [[ -d "${TAB}/icons" ]]; then
  find "${TAB}/icons" -maxdepth 1 -type f -name '*.svg' -exec cp -f {} "${ROOT}/icons/tabler/" \;
fi
if [[ -f "${TAB}/LICENSE" ]]; then
  cp -f "${TAB}/LICENSE" "${ROOT}/icons/tabler/LICENSE"
fi

curl -fsSL -o phosphor.tgz https://codeload.github.com/phosphor-icons/core/tar.gz/refs/heads/main
tar -xzf phosphor.tgz
PH=$(find . -maxdepth 1 -type d -name 'core-*' | head -1)
if [[ -d "${PH}/assets/regular" ]]; then
  find "${PH}/assets/regular" -type f -name '*.svg' -exec cp -f {} "${ROOT}/icons/phosphor/" \;
elif [[ -d "${PH}/raw/regular" ]]; then
  find "${PH}/raw/regular" -type f -name '*.svg' -exec cp -f {} "${ROOT}/icons/phosphor/" \;
fi
if [[ -f "${PH}/LICENSE" ]]; then
  cp -f "${PH}/LICENSE" "${ROOT}/icons/phosphor/LICENSE"
elif [[ -f "${PH}/LICENSE.txt" ]]; then
  cp -f "${PH}/LICENSE.txt" "${ROOT}/icons/phosphor/LICENSE"
fi

echo "lucide=$(find "${ROOT}/icons/lucide" -name '*.svg' | wc -l)"
echo "tabler=$(find "${ROOT}/icons/tabler" -name '*.svg' | wc -l)"
echo "phosphor=$(find "${ROOT}/icons/phosphor" -name '*.svg' | wc -l)"
du -sh "${ROOT}/icons/lucide" "${ROOT}/icons/tabler" "${ROOT}/icons/phosphor"
ls -la "${ROOT}/icons/lucide/LICENSE" "${ROOT}/icons/tabler/LICENSE" "${ROOT}/icons/phosphor/LICENSE" "${ROOT}/third_party/nanosvg.h" "${ROOT}/third_party/nanosvgrast.h"
rm -rf "${ROOT}/tmp_icons"
echo DONE
