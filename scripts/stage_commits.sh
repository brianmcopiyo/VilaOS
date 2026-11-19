#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

commit_staged() {
  local msg="$1"
  if git diff --cached --quiet; then
    echo "skip empty: ${msg}"
    return 0
  fi
  git commit -m "${msg}"
  echo "committed: ${msg}"
}

echo "=== 1/5 icon libraries ==="
git add icons/ third_party/nanosvg.h third_party/nanosvgrast.h scripts/fetch_icons.sh
commit_staged "Bundle Lucide, Tabler, and Phosphor SVG icon sets with licenses."

echo "=== 2/5 icon rasterizer ==="
git add include/icon.h src/gfx/icon.c include/hwinfo.h src/gfx/hwinfo.c include/types.h \
  Makefile src/main.c scripts/rebuild_iso_quick.sh scripts/build_qemu_image.sh
commit_staged "Add on-demand SVG icon rasterization with RAM-tiered cache and ISO packing."

echo "=== 3/5 single-word components ==="
git add scripts/rename_components.sh \
  sdk/ui/ sdk/components/ sdk/kotlin/ui/Ui.kt sdk/ktc sdk/templates/app/src/main.kt \
  assets/ui/ assets/components/ assets/screens/ \
  src/xml_ui/ src/ui/screen.c \
  apps/*/res/components/ apps/browser/res/components/ apps/calculator/res/components/ \
  apps/clock/res/components/ apps/contacts/res/components/ apps/dialer/res/components/ \
  apps/files/res/components/ apps/messages/res/components/ apps/music/res/components/ \
  apps/notes/res/components/ apps/settings/res/components/
git add -u sdk/ui sdk/components assets/ui assets/components assets/screens \
  apps/*/res/components sdk/kotlin/ui/Ui.kt sdk/ktc sdk/templates
commit_staged "Standardize UI components to single-word SDK and XML names."

echo "=== 4/5 app wiring ==="
git add apps/browser apps/calculator apps/clock apps/contacts apps/dialer \
  apps/files apps/messages apps/music apps/notes
git add -u apps/browser apps/calculator apps/clock apps/contacts apps/dialer \
  apps/files apps/messages apps/music apps/notes
commit_staged "Wire apps to icon attributes and single-word UI components."

echo "=== 5/5 settings + system APIs ==="
git add apps/settings src/api/vapi.c src/api/vapi_system.c sdk/kotlin/system/System.kt
git add -u apps/settings src/api/vapi.c src/api/vapi_system.c sdk/kotlin/system/System.kt
commit_staged "Replace Settings with multi-section OS controls and richer system APIs."

echo "=== leftover check ==="
git add -A
if git diff --cached --quiet; then
  echo "nothing left to commit"
else
  echo "remaining files:"
  git diff --cached --name-only | head -100
  commit_staged "Capture remaining VilaOS workspace changes."
fi

echo
git log --oneline -8
echo
git status -sb
'
