@echo off

if "%1" == "full" (
    rd /s /q build_msvc
    mkdir build_msvc
    cd build_msvc
    cmake ..

    cmake --build . --config Release

    cd ..
) else if "%1" == "fast" (
    cd build_msvc
    cmake --build . --config Release
    cd ..
) else if "%1" == "clean" (
    rd /s /q build_msvc
) else (
    echo "Usage: build_msvc.bat [full|fast|clean]"
)
