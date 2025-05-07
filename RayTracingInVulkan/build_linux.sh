#!/bin/sh
set -e

rm -rf build/linux/assets/shaders &&
mkdir --parents build/linux
cd build/linux
cmake -D CMAKE_BUILD_TYPE=Release -D VCPKG_TARGET_TRIPLET=x64-linux -D CMAKE_TOOLCHAIN_FILE=../vcpkg.linux/scripts/buildsystems/vcpkg.cmake -D AllowProcedurals=ON ../..
make -j
