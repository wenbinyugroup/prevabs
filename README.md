# PreVABS

PreVABS is a preprocessing tool for VABS and SwiftComp.
It helps users to create cross-sections and write input files.

## Usage

````
prevabs -i <main_input.xml> [options]
````

Options

````
Analysis options
----------------

-h      Build cross section and generate VABS/SwiftComp input file for homogenization.
-d      Read 1D beam analysis results and update VABS/SwiftComp input file for dehomogenization.
-fi     Initial failure indices and strength ratios.
-f      Initial failure strength analysis (SwiftComp only).
-fe     Initial failure envelope (SwiftComp only).

Format and execution options
----------------------------

-vabs   Use VABS format (Default).
-sc     Use SwiftComp format.
-int    Use integrated solver.
-e      Execute VABS/SwiftComp.
-v      Visualize meshed cross section for homogenization or contour plots of stresses and strains after recovery.
-debug  Debug mode.
````

## Build from source

### Requirements

* Compiler: support C++11 (For GCC, need 4.7 or higher)
  * C
  * C++
  * Fortran
* CMake 3.0 or higher
* Boost 1.66 or higher
* Libraries
  * blas/lapack
  * gfortran

### Linux

1. Unpack the PreVABS package and change the directory to the package
root. In the following steps, `<PREVABS_DIR>` stands for the absolute
directory of the package (`/.../prevabs-0.6-source`).

    ````
    tar -xzvf prevabs-0.6-source.tar.gz
    cd prevabs-0.6-source
    ````

2. The default compiler is GCC. To use a different compiler, open the
file `build_all_linux_64.sh` and edit the compiler options
`-DCMAKE_C_COMPILER`, `-DCMAKE_CXX_COMPILER` and `-DCMAKE_Fortran_COMPILER`
(there are two places for this setting, one for Gmsh and the other for PreVABS).

3. Run `build_all_linux_64.sh`.

    ````
    ./build_all_linux_64.sh
    ````

4. Add `<PREVABS_DIR>/bin` to the environment variables `PATH` and `<PREVABS_DIR>/lib`
    to `LD_LIBRARY_PATH`. You can also move files in these locations to
    proper directories in your system.

    ````
    export PATH=<PREVABS_DIR>/bin:$PATH
    export LD_LIBRARY_PATH=<PREVABS_DIR>/lib:$LD_LIBRARY_PATH
    ````

### Windows

#### Environment: Cygwin

1. Download Cygwin
2. Install the following tools and packages



#### Environment: MSYS2 MingGW 64

1. Download and install MSYS2 from https://www.msys2.org/. Follow the steps outlined there to update MSYS2 itself.
2. Install necessary packages:

   ````bash
   pacman -S mingw-w64-x86_64-toolchain
   pacman -S mingw-w64-x86_64-cmake
   ````

3. Open a MinGW-w64 shell:

   1. To build 32-bit things, open the "MinGW-w64 32-bit Shell"
   2. To build 64-bit things, open the "MinGW-w64 64-bit Shell"

4. Build with the script 

    ````bash
    ./build_all_mingw64_msys2.sh
    ````

**NOTE**: There is a known issue that if the lapack package 
(`mingw-w64-x86_64-lapack`) is installed, there will be the following
linking errors:

````bash
C:/msys64/mingw64/bin/../lib/gcc/x86_64-w64-mingw32/10.2.0/../../../../x86_64-w64-mingw32/bin/ld.exe: C:/msys64/mingw64/bin/../lib/gcc/x86_64-w64-mingw32/10.2.0\libgfortran.a(write.o):(.text$determine_en_precision+0x1f7): undefined reference to `quadmath_snprintf'
C:/msys64/mingw64/bin/../lib/gcc/x86_64-w64-mingw32/10.2.0/../../../../x86_64-w64-mingw32/bin/ld.exe: C:/msys64/mingw64/bin/../lib/gcc/x86_64-w64-mingw32/10.2.0\libgfortran.a(write.o):(.text$get_float_string+0x265): undefined reference to `quadmath_snprintf'
C:/msys64/mingw64/bin/../lib/gcc/x86_64-w64-mingw32/10.2.0/../../../../x86_64-w64-mingw32/bin/ld.exe: C:/msys64/mingw64/bin/../lib/gcc/x86_64-w64-mingw32/10.2.0\libgfortran.a(write.o):(.text$get_float_string+0xdc3): undefined reference to `quadmath_snprintf'
C:/msys64/mingw64/bin/../lib/gcc/x86_64-w64-mingw32/10.2.0/../../../../x86_64-w64-mingw32/bin/ld.exe: C:/msys64/mingw64/bin/../lib/gcc/x86_64-w64-mingw32/10.2.0\libgfortran.a(write.o):(.text$get_float_string+0x1753): undefined reference to `quadmath_snprintf'
C:/msys64/mingw64/bin/../lib/gcc/x86_64-w64-mingw32/10.2.0/../../../../x86_64-w64-mingw32/bin/ld.exe: C:/msys64/mingw64/bin/../lib/gcc/x86_64-w64-mingw32/10.2.0\libgfortran.a(write.o):(.text$get_float_string+0x188e): undefined reference to `quadmath_snprintf'
collect2.exe: error: ld returned 1 exit status
mingw32-make[3]: *** [CMakeFiles/shared.dir/build.make:3767: libGmsh.dll] Error 1
mingw32-make[2]: *** [CMakeFiles/Makefile2:921: CMakeFiles/shared.dir/all] Error 2
mingw32-make[1]: *** [CMakeFiles/Makefile2:928: CMakeFiles/shared.dir/rule] Error 2
mingw32-make: *** [Makefile:528: shared] Error 2
````

### Test

Change to the test directory and run the example.

````
cd <PREVABS_DIR>/share/test/vr7
prevabs -i vr7.xml -h -v
````


## Contact

Su Tian
(tian50@purdue.edu)

Haodong Du
(duhd1993@purdue.edu)

Wenbin Yu
(wenbinyu@purdue.edu)
