@echo on

rd /s /q build_msvc
mkdir build_msvc
cd build_msvc
cmake ..

cmake --build . --config Release

cd ..
