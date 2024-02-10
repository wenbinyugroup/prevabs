#!/bin/bash
cd gmsh-3.0.6-source
mkdir build_linux_64
cd build_linux_64

cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH="/package/gcc/7.1.0" -DCMAKE_BUILD_TYPE=Release -DDEFAULT=0 -DENABLE_ANN=1 -DENABLE_BUILD_SHARED=1 -DENABLE_CXX11=1 -DENABLE_FLTK=0 -DENABLE_MESH=1 -DENABLE_OCC=0 -DENABLE_PARSER=1 -DENABLE_POST=1 -DCMAKE_INSTALL_PREFIX="../.." ..

make shared -j4
make install/fast

cd ../..
mkdir build_linux_64
cd build_linux_64

cmake3 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_Fortran_COMPILER=gfortran -DCMAKE_PREFIX_PATH="/package/gcc/7.1.0" -DVABS_INSTALL_DIR="/home/msg/a/MSGCodes" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=.. -DCMAKE_VERBOSE_MAKEFILE=0 ..

make install

cd ..
cp MaterialDB.xml bin/MaterialDB.xml
