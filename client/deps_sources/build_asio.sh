#!/bin/bash

local_path=$(cd "$(dirname "$0")"; pwd)


#curl -O https://github.com/openssl/openssl/releases/download/openssl-3.6.3/openssl-3.6.3.tar.gz
#tar zxvf openssl-3.6.3.tar.gz

cd asio-1.38.2

prefix=$local_path/../deps
echo $prefix

export CFLAGS="-mmacosx-version-min=12.0"
export LDFLAGS="-mmacosx-version-min=12.0"
# 安装到自定义目录，不要装到 /usr
./configure --prefix=$prefix 
make
make install