@ECHO ON
mkdir build_mingw64
cd build_mingw64
cmake -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_Fortran_COMPILER=x86_64-w64-mingw32-gfortran -DCMAKE_PREFIX_PATH="C:/cygwin64/usr/x86_64-w64-mingw32/sys-root/mingw" -DCMAKE_BUILD_TYPE=Release -DDEFAULT=0 -DENABLE_ANN=1 -DENABLE_BUILD_SHARED=1 -DENABLE_CXX11=1 -DENABLE_FLTK=0 -DENABLE_MESH=1 -DENABLE_OCC=0 -DENABLE_PARSER=1 -DENABLE_POST=1 -DCMAKE_INSTALL_PREFIX="../.." .. -G"Unix Makefiles"
