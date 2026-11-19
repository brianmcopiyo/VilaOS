#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../apps/settings" && pwd)"
rm -f "${ROOT}/src/main.kt" "${ROOT}/res/layout/apps.xml" "${ROOT}/ui/apps.xml"
mkdir -p "${ROOT}/ui"
cp -f "${ROOT}/res/layout/"*.xml "${ROOT}/ui/"
printf '%s\n' "VilaOS Settings" "Display Input Packages Services System" "Sunset Orange #EA580C" "Warm Beige #F5EFE6" > "${ROOT}/res/assets/about.txt"
cat > "${ROOT}/manifest.xml" << 'EOF'
<?xml version="1.0"?>
<app id="settings" name="Settings" version="3.0.0" version_code="3" icon="settings">
 <summary text="OS administration for display, input, packages, services, and system"/>
 <entry screen="res/layout/main.xml" script="bin/main.kbc"/>
 <capability name="ui"/>
 <capability name="system.device"/>
 <capability name="system.power"/>
 <capability name="storage.app"/>
 <capability name="storage.files"/>
 <capability name="audio.bg"/>
</app>
EOF
ls "${ROOT}/src"
ls "${ROOT}/res/layout"
ls "${ROOT}/ui"
