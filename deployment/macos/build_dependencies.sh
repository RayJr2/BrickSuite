#!/bin/bash
# Build only from source. No Homebrew or installed Qt runtime is consumed.
set -euo pipefail
: "${QT_SOURCE:?Set QT_SOURCE to the Qt 6.10.3 source tree}"
: "${OPENSSL_SOURCE:?Set OPENSSL_SOURCE to verified OpenSSL 3.6.4 sources}"
: "${DEPS_ROOT:?Set DEPS_ROOT to an absolute disposable build directory}"
: "${CMAKE:?Set CMAKE to a CMake executable}"
: "${NINJA:?Set NINJA to a Ninja executable}"
ARCH=${ARCH:-arm64}
JOBS=${JOBS:-8}
case "$ARCH" in
    arm64) OPENSSL_PLATFORM=darwin64-arm64-cc ;;
    x86_64) OPENSSL_PLATFORM=darwin64-x86_64-cc ;;
    *) echo "Unsupported architecture: $ARCH" >&2; exit 1 ;;
esac
export PATH="$(dirname "$CMAKE"):$(dirname "$NINJA"):/usr/bin:/bin:/usr/sbin:/sbin"
export MACOSX_DEPLOYMENT_TARGET=13.0
mkdir -p "$DEPS_ROOT/qt-build"
(
    cd "$OPENSSL_SOURCE"
    /usr/bin/perl "$OPENSSL_SOURCE/Configure" "$OPENSSL_PLATFORM" shared no-tests no-module \
        --prefix="$DEPS_ROOT/openssl" --openssldir=/etc/ssl -mmacosx-version-min=13.0
    make -j"$JOBS"
    make install_sw
)
(
    cd "$DEPS_ROOT/qt-build"
    "$QT_SOURCE/configure" -prefix "$DEPS_ROOT/qt" -release -opensource -confirm-license \
        -submodules qtbase,qtsvg,qtimageformats,qtwebsockets \
        -skip qtdeclarative -skip qtshadertools -skip qtlanguageserver \
        -nomake tests -nomake examples -openssl-runtime -no-securetransport \
        -sql-sqlite -no-sql-odbc -no-sql-psql -no-sql-mimer -no-sql-mysql \
        -no-sql-oci -no-sql-db2 -no-sql-ibase -qt-zlib -qt-libpng -qt-libjpeg \
        -qt-pcre -qt-doubleconversion -qt-sqlite -no-icu -- \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
        -DOPENSSL_ROOT_DIR="$DEPS_ROOT/openssl" -DFEATURE_system_webp=OFF \
        -DFEATURE_system_tiff=OFF -DFEATURE_system_freetype=OFF \
        -DFEATURE_system_harfbuzz=OFF -DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew
    "$CMAKE" --build . --parallel "$JOBS"
    "$CMAKE" --install .
)
