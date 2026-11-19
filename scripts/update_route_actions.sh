#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

for f in "${ROOT}"/apps/*/res/layout/*.xml; do
  sed -i 's/\r$//' "$f"
  sed -i \
    -e 's|nav:res/layout/|route:push:res/layout/|g' \
    -e 's|right_action="back"|right_action="route:pop"|g' \
    -e 's|action="end_call"|action="route:pop"|g' \
    -e 's|right_action="end_call"|right_action="route:pop"|g' \
    "$f"
done

for f in "${ROOT}"/apps/*/src/*.vsl; do
  sed -i 's/\r$//' "$f"
  sed -i \
    -e 's|^  nav res/layout/|  push_route res/layout/|g' \
    -e 's|^  back$|  pop_route|g' \
    "$f"
done

sed -i 's|right_action="clear"|right_action="clear"|' "${ROOT}/apps/dialer/res/layout/main.xml"

cat > "${ROOT}/apps/dialer/src/main.vsl" << 'EOF'
on_load
  require telephony
end

on_click d0
  digit 0
end
on_click d1
  digit 1
end
on_click d2
  digit 2
end
on_click d3
  digit 3
end
on_click d4
  digit 4
end
on_click d5
  digit 5
end
on_click d6
  digit 6
end
on_click d7
  digit 7
end
on_click d8
  digit 8
end
on_click d9
  digit 9
end
on_click dsta
  digit *
end
on_click dhash
  digit #
end

on_soft left
  action call
end

on_soft cente
  action call
end

on_soft right
  action clea
end
EOF

cat > "${ROOT}/apps/messages/src/main.vsl" << 'EOF'
on_load
  require messages
end

on_click m1
  push_route res/layout/thread.xml
end
on_click m2
  push_route res/layout/thread.xml
end
on_click m3
  push_route res/layout/thread.xml
end
on_click m4
  push_route res/layout/thread.xml
end
on_click newmsg
  push_route res/layout/compose.xml
end

on_soft left
  push_route res/layout/compose.xml
end

on_soft right
  pop_route
end
EOF

cat > "${ROOT}/apps/contacts/src/main.vsl" << 'EOF'
on_load
  require contacts
end

on_click c1
  push_route res/layout/detail.xml
end
on_click c2
  push_route res/layout/detail.xml
end
on_click c3
  push_route res/layout/detail.xml
end
on_click c4
  push_route res/layout/detail.xml
end
on_click c5
  push_route res/layout/detail.xml
end

on_soft left
  action call
end

on_soft right
  pop_route
end
EOF

for app in notes calculator clock files browser settings; do
  cat > "${ROOT}/apps/${app}/src/main.vsl" << EOF
on_load
  require storage
end

on_soft right
  pop_route
end
EOF
done

cat > "${ROOT}/apps/notes/src/main.vsl" << 'EOF'
on_load
  require storage
  storage_get note1 note
end

on_soft left
  storage_set note1 text:note
  action save_note
end

on_soft right
  pop_route
end
EOF

cat > "${ROOT}/apps/calculator/src/main.vsl" << 'EOF'
on_load
  require display
end

on_soft left
  action calc:clea
end

on_soft cente
  action calc:eq
end

on_soft right
  pop_route
end
EOF

cat > "${ROOT}/apps/clock/src/main.vsl" << 'EOF'
on_load
  require clock
end

on_soft left
  action sw_start
end

on_soft right
  pop_route
end
EOF

cat > "${ROOT}/apps/files/src/main.vsl" << 'EOF'
on_load
  require storage
end

on_soft left
  action preview
end

on_soft right
  pop_route
end
EOF

cat > "${ROOT}/apps/browser/src/main.vsl" << 'EOF'
on_load
  require storage
  set_text page_view "Select a page to read"
end

on_click p1
  set_text page_view "Welcome to VilaOS. Apps install as packages under /apps."
end
on_click p2
  set_text page_view "manifest.xml declares id, version, entry UI/script, capabilities."
end
on_click p3
  set_text page_view "VSL: on_click, on_soft, set_text, push_route, pop_route, require."
end

on_soft right
  pop_route
end
EOF

cat > "${ROOT}/apps/settings/src/main.vsl" << 'EOF'
on_load
  require display
end

on_soft right
  pop_route
end
EOF

echo OK
