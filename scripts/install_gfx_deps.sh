#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq \
  pkg-config \
  build-essential \
  autoconf automake libtool pkg-config \
  zlib1g-dev \
  libcairo2-dev \
  libpixman-1-dev \
  libfreetype-dev \
  libpng-dev \
  libdrm-dev \
  libevdev-dev \
  libgbm-dev \
  libegl-dev \
  libgles2-dev \
  wget xz-utils ta
pkg-config --exists cairo pixman-1 freetype2 libpng libdrm libevdev
for p in cairo pixman-1 freetype2 libpng libdrm libevdev gbm egl glesv2; do
  if pkg-config --exists "$p" 2>/dev/null; then
    echo "$p: $(pkg-config --modversion "$p")"
  else
    echo "$p: optional missing"
  fi
done
