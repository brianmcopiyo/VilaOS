#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

for app_dir in "${ROOT}"/apps/*; do
  [[ -d "${app_dir}" ]] || continue
  id="$(basename "${app_dir}")"
  mkdir -p "${app_dir}/res/layout" "${app_dir}/res/assets" "${app_dir}/src"
  if [[ -d "${app_dir}/ui" ]]; then
    cp -a "${app_dir}/ui/." "${app_dir}/res/layout/"
    rm -rf "${app_dir}/ui"
  fi
  if [[ -d "${app_dir}/logic" ]]; then
    cp -a "${app_dir}/logic/." "${app_dir}/src/"
    rm -rf "${app_dir}/logic"
  fi
  if [[ ! -f "${app_dir}/res/assets/about.txt" ]]; then
    printf '%s\n' "${id} package assets" > "${app_dir}/res/assets/about.txt"
  fi
  if [[ -f "${app_dir}/manifest.xml" ]]; then
    sed -i \
      -e 's|screen="ui/main.xml"|screen="res/layout/main.xml"|g' \
      -e 's|script="logic/main.vsl"|script="src/main.vsl"|g' \
      "${app_dir}/manifest.xml"
  fi
  find "${app_dir}/res/layout" -name '*.xml' -print0 2>/dev/null | while IFS= read -r -d '' f; do
    sed -i \
      -e 's|nav:ui/|nav:res/layout/|g' \
      -e 's|file:///apps/browser/ui/main.xml|file:///apps/browser/res/layout/main.xml|g' \
      "$f"
  done
  find "${app_dir}/src" -name '*.vsl' -print0 2>/dev/null | while IFS= read -r -d '' f; do
    sed -i \
      -e 's|nav ui/|nav res/layout/|g' \
      -e 's|ui/call_active.xml|res/layout/call_active.xml|g' \
      "$f"
  done
  echo "migrated ${id}"
done
