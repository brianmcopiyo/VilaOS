#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
L="${ROOT}/apps/launcher"
mkdir -p "${L}/src" "${L}/res/layout" "${L}/res/components" "${L}/res/values" "${L}/res/assets" "${L}/ui" "${L}/bin"
for f in tile row badge button header input text icon divider; do
  if [[ -f "${ROOT}/sdk/ui/${f}.xml" ]]; then
    cp -f "${ROOT}/sdk/ui/${f}.xml" "${L}/res/components/${f}.xml"
  elif [[ -f "${ROOT}/apps/settings/res/components/${f}.xml" ]]; then
    cp -f "${ROOT}/apps/settings/res/components/${f}.xml" "${L}/res/components/${f}.xml"
  fi
done
cp -f "${ROOT}/apps/settings/res/values/colors.xml" "${L}/res/values/colors.xml"
cp -f "${ROOT}/apps/settings/res/values/dimens.xml" "${L}/res/values/dimens.xml"
printf '%s\n' "VilaOS Launcher" "Lock Home Drawer Recents Shade" > "${L}/res/assets/about.txt"
echo "scaffolded ${L}"
