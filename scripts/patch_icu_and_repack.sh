#!/usr/bin/env bash
set -euo pipefail
HOST="/mnt/c/Users/HP/Desktop/OS"
WORK="/root/vilaos-work/src"
ROOTFS="${WORK}/build/rootfs"
find "${HOST}/scripts" -name '*.sh' -exec sed -i 's/\r$//' {} +
rsync -a "${HOST}/scripts/vilaos-chromium.sh" "${WORK}/scripts/"
install -m 755 "${HOST}/scripts/vilaos-chromium.sh" "${ROOTFS}/bin/vilaos-chromium"

python3 - <<'PY'
from pathlib import Path
import re
src = Path("/mnt/c/Users/HP/Desktop/OS/scripts/rebuild_iso_quick.sh").read_text().replace("\r", "")
rootfs = Path("/root/vilaos-work/src/build/rootfs")
m = re.search(r'cat > "\$\{ROOTFS\}/bin/vilaos-chrome-setup" << "EOF"\n(.*?)\nEOF', src, re.S)
(rootfs / "bin/vilaos-chrome-setup").write_text(m.group(1) + "\n")
m = re.search(r'cat > "\$\{ROOTFS\}/init" << "EOF"\n(.*?)\nEOF', src, re.S)
(rootfs / "init").write_text(m.group(1) + "\n")
print("patched setup+init")
PY
chmod 755 "${ROOTFS}/bin/vilaos-chrome-setup" "${ROOTFS}/init" "${ROOTFS}/bin/vilaos-chromium"

cd "${WORK}"
bash scripts/restage_chromium_iso.sh
