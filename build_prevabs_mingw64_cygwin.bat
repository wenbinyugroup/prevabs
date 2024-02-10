@echo on

mkdir build_mingw64
cd build_mingw64
cmake ^
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc ^
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ ^
  -DCMAKE_Fortran_COMPILER=x86_64-w64-mingw32-gfortran ^
  -DCMAKE_PREFIX_PATH="C:/cygwin64/usr/x86_64-w64-mingw32/sys-root/mingw" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX=.. ^
  -DCMAKE_VERBOSE_MAKEFILE=1 ^
  -DVABS_INSTALL_DIR="C:/Program Files/AnalySwift" ^
  .. -G"Unix Makefiles"

make install

cd ..
@REM rm -r gmsh-3.0.6-source/build_mingw64
@REM rm -r build_mingw64
cp lib/*Gmsh*.dll bin/
cp MaterialDB.xml bin/MaterialDB.xml

