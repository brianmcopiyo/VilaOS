#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
for f in "${ROOT}"/apps/*/ui/*.xml; do
  sed -i 's/\r$//' "$f"
  sed -i \
    -e 's|nav:assets/screens/message_thread.xml|nav:ui/thread.xml|g' \
    -e 's|nav:assets/screens/compose.xml|nav:ui/compose.xml|g' \
    -e 's|nav:assets/screens/contact_detail.xml|nav:ui/detail.xml|g' \
    -e 's|nav:assets/screens/messages.xml|back|g' \
    -e 's|nav:assets/screens/contacts.xml|back|g' \
    -e 's|nav:assets/screens/dialer.xml|back|g' \
    -e 's|nav:assets/screens/home.xml|back|g' \
    "$f"
done
grep -n '<screen' "${ROOT}/apps/messages/ui/thread.xml" | head
echo OK
