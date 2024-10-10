#!/bin/bash
mkdir build_linux_64
cd build_linux_64
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH="/usr/lib64" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=.. ..
