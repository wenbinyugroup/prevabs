#!/bin/bash

# cd gmsh-3.0.6-source
# mkdir build_mingw64
# cd build_mingw64
# cmake -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_Fortran_COMPILER=x86_64-w64-mingw32-gfortran -DCMAKE_MAKE_PROGRAM=mingw32-make -DCMAKE_BUILD_TYPE=Release -DDEFAULT=0 -DENABLE_ANN=1 -DENABLE_BUILD_SHARED=1 -DENABLE_CXX11=1 -DENABLE_FLTK=0 -DENABLE_MESH=1 -DENABLE_OCC=0 -DENABLE_PARSER=1 -DENABLE_POST=1 -DCMAKE_INSTALL_PREFIX="../.." .. -G"MSYS Makefiles"

# mingw32-make shared -j4
# mingw32-make install/fast
# cp libGmsh.dll.a ../../lib/
# cd ../..

mkdir build_mingw64
cd build_mingw64
cp ../gmsh-3.0.6-source/build_mingw64/libGmsh.dll.a .
cmake -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_MAKE_PROGRAM=mingw32-make -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=.. .. -G"MSYS Makefiles"

mingw32-make install

cd ..
# rm -r gmsh-3.0.6-source/build_mingw64
# rm -r build_mingw64
cp lib/*.dll bin/
cp MaterialDB.xml bin/MaterialDB.xml

