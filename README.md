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

* Compiler
  * C
  * C++ (11)
  * Fortran (required to link to VABS shared library)
* CMake 3.0 or higher
* Libraries
  * [boost](https://www.boost.org/users/download/)
  * blas/lapack
  * [gmsh sdk](https://gmsh.info/)



### Linux

1. Unpack the PreVABS package and change the directory to the package root.
In the following steps, `<PREVABS_DIR>` stands for the absolute directory of the package (`/.../prevabs-<version>-source`).

    ````
    tar -xzvf prevabs-<version>-source.tar.gz
    cd prevabs-<version>-source
    ````

2. The default compiler is GCC.
To use a different compiler, open the file `build_all_linux_64.sh` and edit the compiler options `-DCMAKE_C_COMPILER`, `-DCMAKE_CXX_COMPILER` and `-DCMAKE_Fortran_COMPILER` (there are two places for this setting, one for Gmsh and the other for PreVABS).

3. Run `build_all_linux_64.sh`.

    ````
    ./build_all_linux_64.sh
    ````

4. Add `<PREVABS_DIR>/bin` to the environment variables `PATH` and `<PREVABS_DIR>/lib` to `LD_LIBRARY_PATH`.
You can also move files in these locations to proper directories in your system.

    ````
    export PATH=<PREVABS_DIR>/bin:$PATH
    export LD_LIBRARY_PATH=<PREVABS_DIR>/lib:$LD_LIBRARY_PATH
    ````

### Windows

#### Environment: Visual Studio

* Step 1: Setup Boost
  * 1.1 Download from https://www.boost.org/users/download/
  * 1.2 Unzip the package to a directory
  * 1.3 Build Boost libraries
    * 1.3.1 Open "Developer Command Prompt for VS 2022"
    * 1.3.2 Change directory to the root of Boost
    * 1.3.3 Run `bootstrap`
    * 1.3.4 Run `b2 link=shared`
  * 1.4 Add a new environment variable `Boost_ROOT` and set it to the root directory of boost
* Step 2: Setup Gmsh
  * 2.1 Download Gmsh SDK from https://gmsh.info/
  * 2.2 Unzip the package to a directory
  * 2.3 Modify the header file
    * 2.3.1 Go to `include`
    * 2.3.2 Rename `gmsh.h` to `gmsh.h_cpp`
    * 2.3.3 Rename `gmsh.h_cwrap` to `gmsh.h`
  * 2.4 Add a new environment variable `Gmsh_ROOT` and set it to the root directory of gmsh
* Step 3: Build PreVABS
  * 3.1 Open "Developer Command Prompt for VS 2022"
  * 3.2 Change the directory to the root of PreVABS
  * 3.3 Run `tools\build_msvc.bat full`

Once done, the executable `prevabs.exe` can be found in `build_msvc\Release\`.



## Contact

Su Tian
(tian50@purdue.edu)

Haodong Du
(duhd1993@purdue.edu)

Wenbin Yu
(wenbinyu@purdue.edu)
