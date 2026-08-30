#!/bin/bash

local_path=$(cd "$(dirname "$0")"; pwd)

deps_dir=$local_path/../deps

cd jsoncpp-1.9.8
mkdir build
cd build



cmake ../ -DBUILD_STATIC_LIBS=ON \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_INSTALL_PREFIX=$deps_dir \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DCMAKE_BUILD_TYPE=Release

make -j4 install