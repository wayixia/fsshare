#!/bin/bash




cd libwebsockets
mkdir build
cd build
cmake ../ -DLWS_WITH_HTTP2=1 \
      -DLWS_WITHOUT_TEST_SERVER=1 \
      -DLWS_WITHOUT_TESTAPPS=1 \
      -DLWS_WITH_GNUTLS=0 \
      -DLWS_WITH_SSL=1 \
      -DCMAKE_INSTALL_PREFIX=`pwd`/../../../deps \
      -DCMAKE_BUILD_TYPE=Release
make -j4 install