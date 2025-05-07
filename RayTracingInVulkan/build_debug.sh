#!/bin/sh
set -e

mkdir --parents build/debug
cd build/debug
cmake -D CMAKE_BUILD_TYPE=Debug -D VCPKG_TARGET_TRIPLET=x64-linux -D CMAKE_TOOLCHAIN_FILE=../vcpkg.linux/scripts/buildsystems/vcpkg.cmake ../..
make -j
