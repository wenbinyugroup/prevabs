# PreVABS

PreVABS is a preprocessing tool for VABS and SwiftComp.
It helps users to create cross-sections and write input files.

## Installation

### Windows

#### Portable (`prevabs-<ver>-windows-portable.zip`)

- Unzip the package
- Add the root directory to the system environment variable `PATH`

## Usage

````
prevabs -i <main_input.xml> [options]
````

Options

````
Required arguments
------------------

-i, --input <file>       Input XML file (required)

Analysis mode (exactly one required)
------------------------------------

--hm, --homogenization   Homogenization (build cross section and generate input)
--dh, --dehomogenization Dehomogenization (recovery - read 1D beam analysis results)
--fs, --failure-strength Initial failure strength analysis (SwiftComp only)
--fe, --failure-envelope Initial failure envelope (SwiftComp only)
--fi, --failure-index    Initial failure indices and strength ratios

Analysis tool (optional)
-------------------------

--vabs                  Use VABS format (default)
--sc                    Use SwiftComp format

Additional options
------------------

--ver <version>         Tool version (e.g. 3.0)
--integrated            Use integrated solver (implies --execute)
-e, --execute           Execute VABS/SwiftComp after generating input
-v, --visualize         Visualize meshed cross section or contour plots
--gmsh-verbosity <n>    Gmsh log verbosity (0=silent, 1=errors, 2=warnings, 3=info, 5=debug)
-d, --debug             Debug mode
````

## Build from source

### Requirements

* Compiler: C and C++ (C++11)
* CMake 3.14 or higher
* Libraries
  * [Gmsh SDK](https://gmsh.info/)

### Windows (MSVC)

1. **Setup Gmsh**
   - Download the Gmsh SDK from https://gmsh.info/
   - Unzip to a directory
   - In the `include` folder, rename `gmsh.h` → `gmsh.h_cpp` and `gmsh.h_cwrap` → `gmsh.h`
   - Set environment variable `Gmsh_ROOT` to the Gmsh SDK root directory

2. **Build PreVABS**

   ```powershell
   # Full clean build
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\build-msvc.ps1 full

   # Incremental rebuild
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\build-msvc.ps1 fast
   ```

The executable will be at `build_msvc\Release\prevabs.exe`.

### WSL (Windows Subsystem for Linux)

Build the Windows MSVC executable from within WSL without switching to a
Windows terminal. Requires `powershell.exe` to be accessible from WSL
(standard on Windows 10/11).

1. Follow the **Windows (MSVC)** Gmsh setup step above.

2. Run the WSL build script from the repo root:

   ```bash
   # Full clean build
   bash tools/build-wsl.sh full

   # Incremental rebuild (default)
   bash tools/build-wsl.sh fast

   # Remove build directory
   bash tools/build-wsl.sh clean
   ```

The executable will be at `build_msvc\Release\prevabs.exe` (Windows path).

### Linux

1. Unpack the source and change to the package root.

2. The default compiler is GCC. To use a different compiler, edit the
   `-DCMAKE_C_COMPILER` / `-DCMAKE_CXX_COMPILER` options in
   `tools/build_all_linux_64.sh`.

3. Run the build script:

   ```bash
   ./tools/build_all_linux_64.sh
   ```

4. Add the output directories to your environment:

   ```bash
   export PATH=<PREVABS_DIR>/bin:$PATH
   export LD_LIBRARY_PATH=<PREVABS_DIR>/lib:$LD_LIBRARY_PATH
   ```

## Testing

### Unit tests (Catch2)

**Windows (MSVC) — using the build script:**

```powershell
# Full clean build and run all tests
powershell.exe -NoProfile -ExecutionPolicy Bypass -File test\build-unit-tests.ps1 full -Run

# Incremental rebuild and run
pwsh -NoProfile -ExecutionPolicy Bypass -File test\build-unit-tests.ps1 fast -Run

# Build only (no run)
pwsh -NoProfile -ExecutionPolicy Bypass -File test\build-unit-tests.ps1 full
```

The test executable is at `test\unit\build_msvc\Release\test_geo.exe`.

**Manual cmake (any platform):**

```bash
cd test/unit
mkdir build && cd build
cmake ..
cmake --build . --config Release
./Release/test_geo          # run all tests
./Release/test_geo "[geo]"  # run tagged subset
```

### Integration tests (Windows)

```bat
test\run_integration_tests.bat
```

## License

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

## Contact

Su Tian
(sutian@analyswift.com)

Haodong Du
(duhd1993@purdue.edu)

Wenbin Yu
(wenbinyu@purdue.edu)
