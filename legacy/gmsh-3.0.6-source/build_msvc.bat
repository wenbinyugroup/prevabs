@ECHO ON
mkdir build_msvc
cd build_msvc
cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DDEFAULT=0 ^
    -DENABLE_ANN=1 ^
    -DENABLE_BUILD_SHARED=1 ^
    -DENABLE_CXX11=1 ^
    -DENABLE_FLTK=0 ^
    -DENABLE_MESH=1 ^
    -DENABLE_OCC=0 ^
    -DENABLE_PARSER=1 ^
    -DENABLE_POST=1 ^
    -DCMAKE_INSTALL_PREFIX="../.." ^
    ..

cmake --build . --config Release
