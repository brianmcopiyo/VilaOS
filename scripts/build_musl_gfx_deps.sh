#!/usr/bin/env bash
set -euo pipefail

PREFIX="${1:-/root/vilaos-work/musl-gfx}"
SRC="${PREFIX}/src"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
export CC=musl-gcc
export CFLAGS="-O2 -fPIC"
export PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig"
export PATH="/usr/bin:$PATH"

mkdir -p "${SRC}" "${PREFIX}/lib" "${PREFIX}/include"
cd "${SRC}"
exec > >(tee /tmp/musl-gfx-build.log) 2>&1

need() { [[ -f "${PREFIX}/.built_$1" ]]; }
mark() { touch "${PREFIX}/.built_$1"; }

fetch_tar() {
  local url="$1" name="$2" strip="${3:-1}"
  if [[ -d "${SRC}/${name}/.ok" ]] || [[ -f "${SRC}/${name}/configure" ]] || [[ -f "${SRC}/${name}/CMakeLists.txt" ]] || [[ -f "${SRC}/${name}/meson.build" ]]; then
    return 0
  fi
  echo "fetch ${name} from ${url}"
  local tmp="${SRC}/${name}.archive"
  curl -L --fail --retry 3 -o "${tmp}" "${url}"
  rm -rf "${SRC}/${name}"
  mkdir -p "${SRC}/${name}"
  case "${url}" in
    *.tar.xz) tar -xJf "${tmp}" -C "${SRC}/${name}" --strip-components="${strip}" ;;
    *) tar -xzf "${tmp}" -C "${SRC}/${name}" --strip-components="${strip}" ;;
  esac
  rm -f "${tmp}"
}

if ! need zlib; then
  fetch_tar "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz" zlib
  cd "${SRC}/zlib"
  ./configure --prefix="${PREFIX}" --static
  make -j"${JOBS}"
  make install
  mark zlib
fi

if ! need libpng; then
  fetch_tar "https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.43.tar.gz" libpng
  cd "${SRC}/libpng"
  ./configure --prefix="${PREFIX}" --disable-shared --enable-static --with-zlib-prefix="${PREFIX}" \
    CPPFLAGS="-I${PREFIX}/include" LDFLAGS="-L${PREFIX}/lib"
  make -j"${JOBS}"
  make install
  mark libpng
fi

if ! need pixman; then
  fetch_tar "https://www.cairographics.org/releases/pixman-0.42.2.tar.gz" pixman
  cd "${SRC}/pixman"
  ./configure --prefix="${PREFIX}" --disable-shared --enable-static --disable-gtk --disable-libpng
  make -j"${JOBS}"
  make install
  mark pixman
fi

if ! need freetype; then
  fetch_tar "https://download.savannah.gnu.org/releases/freetype/freetype-2.13.2.tar.gz" freetype || \
    fetch_tar "https://gitlab.freedesktop.org/freetype/freetype/-/archive/VER-2-13-2/freetype-VER-2-13-2.tar.gz" freetype
  cd "${SRC}/freetype"
  if [[ ! -f configure ]]; then
    ./autogen.sh || true
  fi
  ./configure --prefix="${PREFIX}" --disable-shared --enable-static --with-zlib=yes --with-png=yes \
    --with-harfbuzz=no --with-brotli=no --with-bzip2=no \
    CPPFLAGS="-I${PREFIX}/include" LDFLAGS="-L${PREFIX}/lib"
  make -j"${JOBS}"
  make install
  mark freetype
fi

if ! need cairo; then
  fetch_tar "https://www.cairographics.org/releases/cairo-1.18.0.tar.xz" cairo
  cd "${SRC}/cairo"
  export pixman_CFLAGS="-I${PREFIX}/include/pixman-1"
  export pixman_LIBS="-L${PREFIX}/lib -lpixman-1"
  export FREETYPE_CFLAGS="-I${PREFIX}/include/freetype2 -I${PREFIX}/include"
  export FREETYPE_LIBS="-L${PREFIX}/lib -lfreetype"
  export png_CFLAGS="-I${PREFIX}/include"
  export png_LIBS="-L${PREFIX}/lib -lpng16"
  export zlib_CFLAGS="-I${PREFIX}/include"
  export zlib_LIBS="-L${PREFIX}/lib -lz"
  rm -rf build
  meson setup build --prefix="${PREFIX}" --default-library=static \
    -Dgtk_doc=false -Dtests=disabled -Dxlib=disabled -Dxcb=disabled -Dquartz=disabled \
    -Dtee=disabled -Dpng=enabled -Dfreetype=enabled -Dfontconfig=disabled -Dzlib=enabled \
    -Dglib=disabled -Dspectre=disabled -Dsymbol-lookup=disabled -Ddwrite=disabled
  meson compile -C build -j "${JOBS}"
  meson install -C build
  mark cairo
fi

if ! need libdrm; then
  fetch_tar "https://gitlab.freedesktop.org/mesa/drm/-/archive/libdrm-2.4.120/drm-libdrm-2.4.120.tar.gz" libdrm
  cd "${SRC}/libdrm"
  rm -rf build
  meson setup build --prefix="${PREFIX}" --default-library=static \
    -Dintel=disabled -Dradeon=disabled -Damdgpu=disabled -Dnouveau=disabled \
    -Dvmwgfx=disabled -Dfreedreno=disabled -Detnaviv=disabled -Dvc4=disabled \
    -Dtests=false -Dman-pages=disabled -Dcairo-tests=disabled -Dvalgrind=disabled
  meson compile -C build -j "${JOBS}"
  meson install -C build
  mark libdrm
fi

if ! need libevdev; then
  fetch_tar "https://gitlab.freedesktop.org/libevdev/libevdev/-/archive/libevdev-1.13.1/libevdev-libevdev-1.13.1.tar.gz" libevdev
  cd "${SRC}/libevdev"
  if [[ ! -f configure ]]; then
    autoreconf -fi || meson setup build --prefix="${PREFIX}" --default-library=static
  fi
  if [[ -d build ]] || [[ -f meson.build && ! -f configure ]]; then
    rm -rf build
    meson setup build --prefix="${PREFIX}" --default-library=static -Dtests=disabled -Ddocumentation=disabled
    meson compile -C build -j "${JOBS}"
    meson install -C build
  else
    ./configure --prefix="${PREFIX}" --disable-shared --enable-static
    make -j"${JOBS}"
    make install
  fi
  mark libevdev
fi

cat > "${PREFIX}/vilaos-gfx.pc.env" << EOF
VILAOS_MUSL_GFX_PREFIX='${PREFIX}'
VILAOS_MUSL_GFX_CFLAGS='-I${PREFIX}/include -I${PREFIX}/include/cairo -I${PREFIX}/include/pixman-1 -I${PREFIX}/include/freetype2 -I${PREFIX}/include/libdrm -I${PREFIX}/include/libevdev-1.0 -DVILAOS_HAVE_LINUX_GFX'
VILAOS_MUSL_GFX_LIBS='-L${PREFIX}/lib -L${PREFIX}/lib/x86_64-linux-gnu -lcairo -lpixman-1 -lfreetype -lpng16 -lz -ldrm -levdev -lm'
EOF
touch "${PREFIX}/.built_cairo"
echo "musl gfx deps ready at ${PREFIX}"
cat "${PREFIX}/vilaos-gfx.pc.env"
