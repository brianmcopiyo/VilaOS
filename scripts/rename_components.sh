#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"

find apps assets sdk -type f -name "*.xml" -print0 | xargs -0 sed -i \
  -e 's/component="launcher_tile"/component="tile"/g' \
  -e 's/component="list_row"/component="row"/g' \
  -e 's/component="file_row"/component="row"/g' \
  -e 's/component="message_row"/component="row"/g' \
  -e 's/component="action_button"/component="button"/g' \
  -e 's/component="secondary_button"/component="button"/g' \
  -e 's/component="notification_pill"/component="badge"/g' \
  -e 's/component="status_badge"/component="badge"/g' \
  -e 's/component="section_header"/component="header"/g' \
  -e 's/<input_field /<input /g' \
  -e 's/<\/input_field>/<\/input>/g' \
  -e 's/<list_view /<list /g' \
  -e 's/<\/list_view>/<\/list>/g' \
  -e 's/<softkey_bar /<footer /g' \
  -e 's/<\/softkey_bar>/<\/footer>/g'

mkdir -p sdk/ui assets/ui sdk/components assets/components

if [[ -f sdk/ui/input_field.xml ]]; then mv -f sdk/ui/input_field.xml sdk/ui/input.xml; fi
if [[ -f sdk/ui/list_view.xml ]]; then mv -f sdk/ui/list_view.xml sdk/ui/list.xml; fi
if [[ -f sdk/ui/softkey_bar.xml ]]; then mv -f sdk/ui/softkey_bar.xml sdk/ui/footer.xml; fi
if [[ -f assets/ui/input_field.xml ]]; then mv -f assets/ui/input_field.xml assets/ui/input.xml; fi
if [[ -f assets/ui/list_view.xml ]]; then mv -f assets/ui/list_view.xml assets/ui/list.xml; fi
if [[ -f assets/ui/softkey_bar.xml ]]; then mv -f assets/ui/softkey_bar.xml assets/ui/footer.xml; fi

cat > sdk/ui/tile.xml << 'EOF'
<?xml version="1.0"?>
<component>
  <button id="tile" class="surface" text="App" icon="app" icon-set="lucide" action="none" width="100%" height="100%"
          background="#EAE0D0" color="#1C1917" radius="10" padding="4" component="tile"/>
</component>
EOF
cat > sdk/ui/row.xml << 'EOF'
<?xml version="1.0"?>
<component>
  <button id="row" class="surface" text="Item" icon="" icon-set="lucide" action="none" width="100%" height="36" radius="8"
          background="#EAE0D0" color="#1C1917" margin="0 0 4 0" padding="6" component="row"/>
</component>
EOF
cat > sdk/ui/badge.xml << 'EOF'
<?xml version="1.0"?>
<component>
  <label id="badge" class="pill" text="Status" icon="" icon-set="lucide" value="2" width="auto" height="24" padding="4" radius="12"
         background="#EA580C" color="#FFFFFF" text-align="center" overflow="ellipsis"
         font-family="Inter" font-size="12" component="badge"/>
</component>
EOF
cat > sdk/ui/header.xml << 'EOF'
<?xml version="1.0"?>
<component>
  <label id="header" text="Section" width="100%" height="22" color="#57534E" padding="2" component="header"/>
</component>
EOF
cat > sdk/ui/button.xml << 'EOF'
<?xml version="1.0"?>
<component>
  <button id="btn" class="btn-primary" text="Continue" icon="" icon-set="lucide" action="ok" width="100%" height="40" radius="12"
          background="#EA580C" color="#FFFFFF" padding="8" component="button"/>
</component>
EOF
cat > sdk/ui/text.xml << 'EOF'
<?xml version="1.0"?>
<component>
  <label id="text" text="" width="100%" height="wrap" color="#1C1917" component="text"/>
</component>
EOF
cat > sdk/ui/icon.xml << 'EOF'
<?xml version="1.0"?>
<component>
  <label id="icon" text="" icon="app" icon-set="lucide" width="24" height="24" component="icon"/>
</component>
EOF

if [[ ! -f sdk/ui/input.xml ]]; then
cat > sdk/ui/input.xml << 'EOF'
<?xml version="1.0"?>
<component>
  <input id="field" text="" width="100%" height="40" radius="10" padding="8"
         background="#EAE0D0" color="#1C1917" border-width="1" border-color="#D4C5B0"
         font-family="JetBrains Mono" font-size="14"/>
</component>
EOF
fi
if [[ ! -f sdk/ui/list.xml ]]; then
cat > sdk/ui/list.xml << 'EOF'
<?xml version="1.0"?>
<component>
  <list id="list" width="100%" height="fill" grow="1" min-height="100" padding="4" gap="4"
        radius="8" background="#EAE0D0" border-width="1" border-color="#D4C5B0" overflow="scroll"/>
</component>
EOF
fi
if [[ ! -f sdk/ui/footer.xml ]]; then
cat > sdk/ui/footer.xml << 'EOF'
<?xml version="1.0"?>
<component>
  <softkeys left="Menu" center="OK" right="Back"
            left-action="menu" center-action="ok" right-action="route:pop"/>
</component>
EOF
fi

for f in tile row badge header button text icon input list footer card keypad; do
  if [[ -f "sdk/ui/${f}.xml" ]]; then
    cp -f "sdk/ui/${f}.xml" "assets/ui/${f}.xml"
    cp -f "sdk/ui/${f}.xml" "sdk/components/${f}.xml"
    cp -f "sdk/ui/${f}.xml" "assets/components/${f}.xml"
  fi
done
if [[ -f assets/components/divider.xml ]]; then
  cp -f assets/components/divider.xml sdk/components/divider.xml
fi

rm -f sdk/components/action_button.xml sdk/components/list_row.xml sdk/components/launcher_tile.xml \
  sdk/components/notification_pill.xml sdk/components/section_header.xml sdk/components/message_row.xml \
  sdk/components/secondary_button.xml sdk/components/input_field.xml \
  assets/components/action_button.xml assets/components/list_row.xml assets/components/launcher_tile.xml \
  assets/components/notification_pill.xml assets/components/section_header.xml assets/components/message_row.xml \
  assets/components/secondary_button.xml assets/components/input_field.xml

for app in apps/*/res/components; do
  [[ -d "${app}" ]] || continue
  for f in button row tile badge header input text icon; do
    cp -f "sdk/ui/${f}.xml" "${app}/${f}.xml"
  done
  if [[ -f assets/components/divider.xml ]]; then
    cp -f assets/components/divider.xml "${app}/divider.xml"
  fi
  rm -f "${app}/action_button.xml" "${app}/list_row.xml" "${app}/launcher_tile.xml" \
    "${app}/notification_pill.xml" "${app}/section_header.xml" "${app}/message_row.xml" \
    "${app}/secondary_button.xml" "${app}/input_field.xml" "${app}/file_row.xml"
done

echo "remaining old names:"
grep -RIn 'launcher_tile\|list_row\|action_button\|notification_pill\|section_header\|message_row\|file_row\|input_field\|softkey_bar\|list_view\|ActionButton\|ListRow\|TextInput\|DialKeypad\|StatusBadge\|AppTile\|SurfaceCard\|SoftkeyBar' \
  apps assets sdk --include='*.xml' --include='*.kt' --include='*.c' --include='*.h' | head -50 || true
echo OK
