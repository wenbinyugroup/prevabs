cd gmsh-3.0.6-source
mkdir build_mingw64
cd build_mingw64
cmake -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_Fortran_COMPILER=x86_64-w64-mingw32-gfortran -DCMAKE_PREFIX_PATH="C:\msys64\mingw64" -DCMAKE_BUILD_TYPE=Release -DDEFAULT=0 -DENABLE_ANN=1 -DENABLE_BUILD_SHARED=1 -DENABLE_CXX11=1 -DENABLE_FLTK=0 -DENABLE_MESH=1 -DENABLE_OCC=0 -DENABLE_PARSER=1 -DENABLE_POST=1 -DCMAKE_INSTALL_PREFIX="../.." .. -G"MSYS Makefiles"

@REM make shared -j4
@REM make install/fast
@REM cp libGmsh.dll.a ../../lib/

@REM cd ../..
@REM mkdir build_mingw64
@REM cd build_mingw64
@REM cp ../gmsh-3.0.6-source/build_mingw64/libGmsh.dll.a .
@REM cmake -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_PREFIX_PATH="C:\msys64\mingw64" -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=.. .. -G"MSYS Makefiles"

@REM make install

@REM cd ..
@REM # rm -r gmsh-3.0.6-source/build_mingw64
@REM # rm -r build_mingw64
@REM cp lib/*.dll bin/
@REM cp MaterialDB.xml bin/MaterialDB.xml

