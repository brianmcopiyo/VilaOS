#!/usr/bin/env bash
set -euo pipefail
for p in cairo pixman-1 freetype2 libpng libdrm libevdev gbm egl glesv2; do
  echo "== $p"
  if pkg-config --exists "$p"; then
    pkg-config --cflags --libs "$p"
  else
    echo MISSING
  fi
done
echo "--- musl lib dir ---"
ls /usr/lib/x86_64-linux-musl 2>/dev/null | head -30 || true
echo "--- packages ---"
dpkg -l 2>/dev/null | grep -E "libcairo|libpixman|libfreetype|libpng|libdrm|libevdev|libgbm|libegl|libgles|musl" | head -50 || true
echo "--- musl-gcc ---"
which musl-gcc || true
musl-gcc -print-file-name=libc.so 2>/dev/null || true
