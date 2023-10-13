#!/bin/sh

# Use this script to run the original CMake configuration process.
# It automatically sets what command-line properties are needed.
# You can then freely build and reconfigure from the build directory.

MY_DIR=$(cd `dirname $0` && pwd)
mkdir -p $MY_DIR/build
cd $MY_DIR/build

echo "Building in $(pwd)"
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain.cmake ..
