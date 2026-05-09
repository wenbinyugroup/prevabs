# PreVABS

PreVABS is a preprocessing tool for VABS and SwiftComp.
It helps users to create cross-sections and write input files.

Online documentation: https://wenbinyugroup.github.io/prevabs/

## Installation

Prebuilt release packages are intended to run without a full source build
toolchain. The sections below describe what is already bundled and what users
still need to install manually.

### Windows

#### Runtime package (`prevabs-<ver>-windows-x64.zip`)

Bundled in the package:

- `prevabs.exe`
- `gmsh-<major>.<minor>.dll`
- `MaterialDB.xml`
- `README.md`
- `LICENSE`
- `LICENSE-gmsh.txt`
- `CREDITS-gmsh.txt`

Manual installation still required:

- Microsoft Visual C++ Redistributable for Visual Studio 2022 (x64), if not
  already installed on the target machine
- VABS and/or SwiftComp, if you want PreVABS to execute those solvers with
  `--execute`

Usage:

- Unzip the package
- Run `prevabs.exe` from the package root, or add that directory to `PATH`

#### Full package (`prevabs-<ver>-windows-x64-full.zip`)

Includes everything in the runtime package, plus:

- `gmsh/gmsh.exe`
- `gmsh/onelab.py` when present in the SDK

This package is useful when users want the standalone Gmsh application in
addition to the PreVABS runtime.

### Linux

#### Ubuntu package (`prevabs-<ver>-linux-ubuntu-x64.tar.gz`)

Bundled in the package:

- `prevabs`
- `libgmsh.so*`
- `MaterialDB.xml`
- `README.md`
- `LICENSE`
- `LICENSE-gmsh.txt`
- `CREDITS-gmsh.txt`

Manual installation still required:

- System runtime libraries required by Gmsh/OpenGL/X11 if they are not already
  present on the machine. On Ubuntu/Debian, these typically include
  `libglu1-mesa`, `libgl1`, `libx11-6`, `libxrender1`, `libxft2`,
  `libxcursor1`, `libxfixes3`, `libxinerama1`, and `libfontconfig1`.
- VABS and/or SwiftComp, if you want PreVABS to execute those solvers with
  `--execute`

#### RHEL package (`prevabs-<ver>-linux-rhel9-x64.tar.gz`)

Bundled in the package:

- `prevabs`
- `libgmsh.so*`
- `MaterialDB.xml`
- `README.md`
- `LICENSE`
- `LICENSE-gmsh.txt`
- `CREDITS-gmsh.txt`

Manual installation still required:

- System runtime libraries required by Gmsh/OpenGL/X11 if they are not already
  present on the machine. On RHEL/Rocky 9, these typically include
  `mesa-libGLU`, `libglvnd-glx`, `libX11`, `libXrender`, `libXft`,
  `libXcursor`, `libXfixes`, `libXinerama`, and `fontconfig`.
- VABS and/or SwiftComp, if you want PreVABS to execute those solvers with
  `--execute`

### Examples package

`prevabs-<ver>-examples.zip` contains only example input files. It does not
contain the executable or runtime libraries.

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
* Git, to fetch the vendored `extern/cpp-terminal` submodule

### Fetch source dependencies

PreVABS vendors
[`cpp-terminal`](https://github.com/jupyter-xeus/cpp-terminal) as the git
submodule `extern/cpp-terminal`, following the upstream recommended
"git submodule + CMake" integration flow. Clone with submodules enabled, or
initialize them after cloning:

```bash
# Fresh clone
git clone --recursive <repo-url>

# Existing clone
git submodule update --init --recursive
```

To refresh only `cpp-terminal` in an existing working tree:

```bash
git submodule update --init --recursive extern/cpp-terminal
```

PreVABS builds `cpp-terminal` as part of the normal CMake build through
`add_subdirectory(extern/cpp-terminal)`. No separate manual build or system
installation step is required for this dependency.

### Windows (MSVC)

1. **Fetch source dependencies**
   - Run `git submodule update --init --recursive`
   - Verify `extern/cpp-terminal` is populated before building

2. **Setup Gmsh**
   - Download the Gmsh SDK from https://gmsh.info/
   - Unzip to a directory
   - No SDK header renaming is needed; PreVABS selects `gmsh.h_cwrap`
     automatically on Windows builds
   - Set environment variable `Gmsh_ROOT` to the Gmsh SDK root directory

3. **Build PreVABS**

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

1. Run `git submodule update --init --recursive`.

2. Follow the **Windows (MSVC)** Gmsh setup step above.

3. Run the WSL build script from the repo root:

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

2. Run `git submodule update --init --recursive`.

3. The default compiler is GCC. To use a different compiler, edit the
   `-DCMAKE_C_COMPILER` / `-DCMAKE_CXX_COMPILER` options in
   `tools/build_linux_64.sh`.

4. Run the build script:

   ```bash
   ./tools/build_linux_64.sh
   ```

5. Add the output directories to your environment:

   ```bash
   export PATH=<PREVABS_DIR>/bin:$PATH
   export LD_LIBRARY_PATH=<PREVABS_DIR>/lib:$LD_LIBRARY_PATH
   ```

### Updating `cpp-terminal`

The repository pins `extern/cpp-terminal` to a specific commit. After pulling
changes from the main repository, resync submodules so your working tree matches
the pinned version:

```bash
git submodule update --init --recursive
```

Use `git submodule update --remote --recursive extern/cpp-terminal` only when
you intentionally want to move PreVABS to a newer upstream `cpp-terminal`
revision and then commit the updated submodule pointer.

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

Release packages also include:

- this project's `LICENSE`
- the bundled Gmsh license and credits files when Gmsh binaries are included

