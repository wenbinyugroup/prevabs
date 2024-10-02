@echo on

mkdir build_msvc
cd build_msvc
cmake ..

cmake --build . --config Release

cd ..
