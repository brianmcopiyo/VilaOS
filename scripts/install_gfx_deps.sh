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
  wget xz-utils tar
pkg-config --exists cairo pixman-1 freetype2 libpng libdrm libevdev
for p in cairo pixman-1 freetype2 libpng libdrm libevdev; do
  echo "$p: $(pkg-config --modversion "$p")"
done
