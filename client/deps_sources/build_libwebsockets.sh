#!/bin/bash

local_path=$(cd "$(dirname "$0")"; pwd)

deps_dir=$local_path/../deps

cd libwebsockets-sai-ffa4c1996
mkdir build
cd build

cmake ../ -DLWS_WITH_HTTP2=ON \
      -DLWS_WITH_HTTP3=OFF \
      -DLWS_WITHOUT_TEST_SERVER=ON \
      -DLWS_WITHOUT_TESTAPPS=ON \
      -DLWS_WITH_STATIC=ON \
      -DLWS_WITH_SSL=ON \
      -DOPENSSL_INCLUDE_DIRS=${deps_dir}/include \
      -DOPENSSL_LIBRARIES="${deps_dir}/lib/libssl.a;${deps_dir}/lib/libcrypto.a" \
      -DLWS_WITH_BORINGSSL=OFF \
      -DLWS_WITH_STATIC=ON \
      -DLWS_WITH_SHARED=OFF \
      -DCMAKE_INSTALL_PREFIX=$deps_dir \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
      -DCMAKE_BUILD_TYPE=Release

make -j4 install