#!/bin/bash

oggurl=http://downloads.xiph.org/releases/ogg/libogg-1.3.2.tar.gz
vorbisurl=http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.5.tar.gz

export MAKE="xcrun make"
export CC="xcrun clang"
export CXX="xcrun clang++"
export LD="xcrun ld"
export AR="xcrun ar"
export RANLIB="xcrun ranlib"
export STRIP="xcrun strip"
export CFLAGS="-arch i386 -arch x86_64 -mmacosx-version-min=10.8"

check_tools() {
    echo "+++ Checking build tools"
    if ! xcrun -f make &>/dev/null; then
        echo "Error: could not execute 'make'. Giving up."
        exit 1
    fi
}

if test ! -f libogg.a; then
    check_tools

    mkdir libogg-build

    echo "+++ Fetching and unpacking $oggurl"
    curl -s $oggurl | (cd libogg-build; tar zx --strip-components 1) || exit

    echo "+++ Configuring libogg"
    (cd libogg-build; ./configure) || exit

    echo "+++ Building libogg"
    (cd libogg-build; $MAKE) || exit

    echo "+++ Collecting libogg build products"
    mkdir ogg
    cp libogg-build/src/.libs/libogg.a .
    cp libogg-build/include/ogg/*.h ogg

    echo "+++ Cleaning up libogg"
    rm -rf libogg-build
fi

if test ! -f libvorbisfile.a; then
    check_tools

    mkdir libvorbis-build

    echo "+++ Fetching and unpacking $vorbisurl"
    curl -s $vorbisurl | (cd libvorbis-build; tar zx --strip-components 1) || exit

    echo "+++ Configuring libvorbis"
    (cd libvorbis-build; ./configure --with-ogg-libraries=$(pwd)/.. --with-ogg-includes=$(pwd)/..) || exit

    echo "+++ Building libvorbis"
    (cd libvorbis-build; $MAKE) || exit

    echo "+++ Collecting libvorbis build products"
    mkdir vorbis
    cp libvorbis-build/lib/.libs/libvorbis{,file}.a .
    cp libvorbis-build/include/vorbis/{codec,vorbisfile}.h vorbis

    echo "+++ Cleaning up libvorbis"
    rm -rf libvorbis-build
fi
