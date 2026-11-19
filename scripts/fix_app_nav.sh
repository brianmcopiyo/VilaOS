#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
for f in "${ROOT}"/apps/*/res/layout/*.xml; do
  [[ -f "$f" ]] || continue
  sed -i 's/\r$//' "$f"
  sed -i \
    -e 's|nav:assets/screens/message_thread.xml|nav:res/layout/thread.xml|g' \
    -e 's|nav:assets/screens/compose.xml|nav:res/layout/compose.xml|g' \
    -e 's|nav:assets/screens/contact_detail.xml|nav:res/layout/detail.xml|g' \
    -e 's|nav:ui/|nav:res/layout/|g' \
    -e 's|nav:assets/screens/messages.xml|back|g' \
    -e 's|nav:assets/screens/contacts.xml|back|g' \
    -e 's|nav:assets/screens/dialer.xml|back|g' \
    -e 's|nav:assets/screens/home.xml|back|g' \
    "$f"
done
echo OK
