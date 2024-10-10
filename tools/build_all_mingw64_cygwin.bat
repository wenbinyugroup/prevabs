@echo on

cd gmsh-3.0.6-source
mkdir build_mingw64
cd build_mingw64
cmake ^
  -D CMAKE_C_COMPILER=x86_64-w64-mingw32-gcc ^
  -D CMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ ^
  -D CMAKE_Fortran_COMPILER=x86_64-w64-mingw32-gfortran ^
  -D CMAKE_PREFIX_PATH=";C:/cygwin64/usr/x86_64-w64-mingw32/sys-root/mingw;" ^
  -D CMAKE_BUILD_TYPE=Release ^
  -D DEFAULT=0 ^
  -D ENABLE_ANN=1 ^
  -D ENABLE_BUILD_SHARED=1 ^
  -D ENABLE_CXX11=1 ^
  -D ENABLE_FLTK=0 ^
  -D ENABLE_MESH=1 ^
  -D ENABLE_OCC=0 ^
  -D ENABLE_PARSER=1 ^
  -D ENABLE_POST=1 ^
  -D CMAKE_INSTALL_PREFIX="../.." ^
  .. -G"Unix Makefiles"

make shared -j2
make install/fast
cp libGmsh.dll.a ../../lib/
cd ../..

mkdir build_mingw64
cd build_mingw64
cmake ^
  -D CMAKE_C_COMPILER=x86_64-w64-mingw32-gcc ^
  -D CMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ ^
  -D CMAKE_Fortran_COMPILER=x86_64-w64-mingw32-gfortran ^
  -D CMAKE_PREFIX_PATH=";C:/cygwin64/usr/x86_64-w64-mingw32/sys-root/mingw;" ^
  -D CMAKE_FIND_USE_CMAKE_SYSTEM_PATH=FALSE ^
  -D CMAKE_BUILD_TYPE=Release ^
  -D CMAKE_INSTALL_PREFIX=.. ^
  -D CMAKE_VERBOSE_MAKEFILE=1 ^
  -D VABS_INSTALL_DIR="C:/Program Files/AnalySwift" ^
  --debug-find ^
  .. -G"Unix Makefiles"

make install

cd ..
@REM rm -r gmsh-3.0.6-source/build_mingw64
@REM rm -r build_mingw64
cp lib/*Gmsh*.dll bin/
cp MaterialDB.xml bin/MaterialDB.xml

