#!/bin/bash
mkdir build_linux_64
cd build_linux_64

cmake3 \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=.. \
  .. \
  --debug-find

make -j4
make install

cd ..
