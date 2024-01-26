#!/bin/bash

oggbase=https://downloads.xiph.org/releases/ogg
oggfile=libogg-1.3.5.tar.gz
oggfilesum=0eb4b4b9420a0f51db142ba3f9c64b333f826532dc0f48c6410ae51f4799b664
vorbisbase=https://downloads.xiph.org/releases/vorbis
vorbisfile=libvorbis-1.3.7.tar.gz
vorbisfilesum=0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab

oggurl=$oggbase/$oggfile
vorbisurl=$vorbisbase/$vorbisfile

# Inherit build architectures from the environment given by Xcode.
arches=()
test -n "$ARCHS" && arches=($ARCHS)
archflags="${arches[*]/#/-arch }"

destdir=$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)

# Determine how many CPU cores the system has so we can parallelise compiles.
ncpus=$(sysctl -q -n hw.ncpu)
makeflags=-j${ncpus:-1}

export MAKE="xcrun make"
export CC="xcrun clang"
export CXX="xcrun clang++"
export LD="xcrun ld"
export AR="xcrun ar"
export RANLIB="xcrun ranlib"
export STRIP="xcrun strip"
export CFLAGS="$archflags -mmacosx-version-min=10.15"

check_tools() {
    echo "+++ Checking build tools"
    if ! xcrun -f make &>/dev/null; then
        echo "Error: could not execute 'make'. Giving up."
        exit 1
    fi
}

check_file() {
    local shasum=$1
    local filename=$2
    shasum -a 256 -c -s - <<EOT
$shasum  $filename
EOT
}

if test ! -f $destdir/out/lib/libogg.a; then
    check_tools

    mkdir libogg-build

    echo "+++ Fetching and unpacking $oggurl"
    if [ -f $oggfile ]; then
        check_file $oggfilesum $oggfile || rm -f $oggfile
    fi
    if [ ! -f $oggfile ]; then
        curl -sL $oggurl -o $oggfile || exit
        check_file $oggfilesum $oggfile || rm -f $oggfile
    fi
    (cd libogg-build; tar zx --strip-components 1) < $oggfile || exit

    echo "+++ Configuring libogg"
    (cd libogg-build; ./configure --prefix=$destdir/out) || exit

    echo "+++ Building libogg"
    (cd libogg-build; $MAKE $makeflags) || exit

    echo "+++ Installing libogg to $destdir"
    (cd libogg-build; $MAKE install) || exit

    # echo "+++ Cleaning up libogg"
    # rm -rf libogg-build
fi

if test ! -f $destdir/out/lib/libvorbisfile.a; then
    check_tools

    mkdir libvorbis-build

    echo "+++ Fetching and unpacking $vorbisurl"
    if [ -f $vorbisfile ]; then
        check_file $vorbisfilesum $vorbisfile || rm -f $vorbisfile
    fi
    if [ ! -f $vorbisfile ]; then
        curl -sL $vorbisurl -o $vorbisfile || exit
        check_file $vorbisfilesum $vorbisfile || rm -f $vorbisfile
    fi
    (cd libvorbis-build; tar zx --strip-components 1) < $vorbisfile || exit

    echo "+++ Patching libvorbis configure"
    sed -i -e 's/-force_cpusubtype_ALL//' libvorbis-build/configure || exit

    echo "+++ Configuring libvorbis"
    (cd libvorbis-build; PKG_CONFIG=/usr/bin/false ./configure --prefix=$destdir/out --with-ogg=$destdir/out) || exit

    echo "+++ Building libvorbis"
    (cd libvorbis-build; $MAKE $makeflags) || exit

    echo "+++ Installing libvorbis to $destdir"
    (cd libvorbis-build; $MAKE install) || exit

    # echo "+++ Cleaning up libvorbis"
    # rm -rf libvorbis-build
fi
