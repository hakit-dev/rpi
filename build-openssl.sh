#!/bin/bash

# Required packages: gcc-arm-linux-gnueabihf

dir=$(readlink -f $(dirname "$0"))

CROSS_PATH="$dir/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64"
CROSS=arm-linux-gnueabihf
#CROSS_ROOT_PATH="$CROSS_PATH/$CROSS"
PREFIX="$CROSS"

export PATH="$CROSS_PATH/bin:$PATH"

version=1.0.2h
srcdir=openssl-$version
tarball=$srcdir.tar.gz
url=https://www.openssl.org/source/$tarball

[ -f $tarball ] || wget $url
[ -d $srcdir ] || tar xfz $tarball

(cd $srcdir && ./Configure dist no-shared --prefix="/$PREFIX" os/compiler:$CROSS)
make -C $srcdir CC="${CROSS}-gcc" AR="${CROSS}-ar r" RANLIB="${CROSS}-ranlib" depend
make -C $srcdir CC="${CROSS}-gcc" AR="${CROSS}-ar r" RANLIB="${CROSS}-ranlib"
make -C $srcdir INSTALL_PREFIX="$CROSS_PATH" install_sw
