@echo on

@REM cd gmsh-3.0.6-source
@REM mkdir build_mingw64
@REM cd build_mingw64
@REM cmake -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_Fortran_COMPILER=x86_64-w64-mingw32-gfortran -DCMAKE_PREFIX_PATH="C:/cygwin64/usr/x86_64-w64-mingw32/sys-root/mingw" -DCMAKE_BUILD_TYPE=Release -DDEFAULT=0 -DENABLE_ANN=1 -DENABLE_BUILD_SHARED=1 -DENABLE_CXX11=1 -DENABLE_FLTK=0 -DENABLE_MESH=1 -DENABLE_OCC=0 -DENABLE_PARSER=1 -DENABLE_POST=1 -DCMAKE_INSTALL_PREFIX="../.." .. -G"Unix Makefiles"

@REM make shared -j2
@REM make install/fast
@REM cp libGmsh.dll.a ../../lib/
@REM cd ../..

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

