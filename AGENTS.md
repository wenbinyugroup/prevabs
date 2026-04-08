# AGENTS.md

## Project Overview

PreVABS is a C++ preprocessing tool for VABS and SwiftComp. It builds cross-section geometry and generates input files for structural analysis.

- **Language**: C++11
- **Build System**: CMake (minimum 3.14)
- **Dependencies**: Gmsh SDK
- **Test Framework**: [Catch2](https://github.com/catchorg/Catch2) (single-header, v3.x)

## First Principles

Use first-principles thinking. Do not assume that I always clearly understand what I want or how to achieve it. Stay cautious and start from the fundamental needs and problems. If the motivation or goal is unclear, stop and discuss it with me.

---

## Build Commands

### Windows (MSVC)

```bash
# Full rebuild with powershell (cleans and rebuilds)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\build-msvc.ps1 full

# Fast rebuild with pwsh (incremental)
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\build-msvc.ps1 fast

# Clean build directory
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\build-msvc.ps1 clean
```

The executable will be in `build_msvc\Release\prevabs.exe`.

### WSL (Windows Subsystem for Linux)

Build the Windows MSVC executable from WSL. Delegates to `build-msvc.ps1`
via `powershell.exe` (available in WSL by default on Windows 10/11).

```bash
# Full clean build
bash tools/build-wsl.sh full

# Incremental rebuild (default)
bash tools/build-wsl.sh fast

# Remove build directory
bash tools/build-wsl.sh clean
```

The executable will be at `build_msvc\Release\prevabs.exe` (Windows path).

### Linux/macOS

```bash
# Using the provided script
./tools/build_all_linux_64.sh

# Or manual CMake build
mkdir build && cd build
cmake ..
make
```

### Running Tests

**Unit tests (Catch2):**

```powershell
# Windows: full clean build and run (from repo root)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File test\build-unit-tests.ps1 full -Run

# Incremental rebuild and run
pwsh -NoProfile -ExecutionPolicy Bypass -File test\build-unit-tests.ps1 fast -Run

# Clean build directory
pwsh -NoProfile -ExecutionPolicy Bypass -File test\build-unit-tests.ps1 clean
```

The test executable is at `test\unit\build_msvc\Release\test_geo.exe`.

**Manual cmake build (Linux / fallback):**

```bash
cd test/unit
mkdir build && cd build
cmake ..
cmake --build . --config Release
./Release/test_geo
```

**Common Catch2 commands:**
```bash
./test_geo                      # Run all tests
./test_geo "[geo]"              # Run tests with tag [geo]
./test_geo "[geo][offset]"      # Run offset-specific tests
./test_geo "test name"          # Run test matching "test name"
./test_geo -s                   # Show successful assertions
./test_geo -l                   # List all test cases
```

**Integration tests:**

```bash
# Run all integration tests
test\run_integration_tests.bat

# Run a specific test manually
prevabs.exe -i test\integration\box\input.xml --hm
```

---

## Code Style Guidelines

- Encourage **modularity** and **reusability** in code design.
- Discourage **monolithic** code design.
- Avoid one giant function/file.
- When creating new functions, classes, modules, etc., start from simplest possible implementation, and gradually add features. **Do not overengineer**.
- Always use numpy style docstrings.
- Use comments to annotate key steps in the code.

- When developing new functions, make use of existing functions as much as possible. Avoid reinventing the wheel.

- When proposing modifications or refactoring plans, you must follow these rules:
  - Do not provide compatibility or workaround-based solutions.
  - Avoid over-engineering; follow the shortest path to implementation while still adhering to the first-principles requirement.
  - Do not introduce solutions beyond the requirements I provided (e.g., fallback or downgrade strategies), as they may cause deviations in business logic.
  - Ensure the logical correctness of the solution; it must be validated through end-to-end reasoning.

### General Rules

- **C++ Standard**: C++11 (set in CMakeLists.txt)
- **Indentation**: 2 spaces (no tabs)
- **Line endings**: Unix-style LF (configure your editor)
- **Max line length**: ~80 characters (soft guideline)
- **No commented-out dead code**: Remove dead code; git history preserves it

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase with 'P' prefix | `PModel`, `PDCELVertex`, `PBaseline` |
| Structs | PascalCase | `LoadCase`, `LocalState` |
| Member variables | Underscore prefix (always) | `_name`, `_global_mesh_size` |
| Global variables | Lowercase with underscores | `debug`, `i_indent`, `config` |
| Functions/methods | camelCase | `initialize()`, `buildGmsh()` |
| Constants | Uppercase with underscores | `SINGLE_LINE` |
| Files | Match class name | `PModel.cpp`, `PModel.hpp` |

### File Organization

```
include/          # Header files (.hpp)
src/              # Source files (.cpp)
  cs/             # Cross-section related
  geo/            # Geometry related
  io/             # Input/output related
  gmsh/           # Gmsh interface
test/
  unit/           # Unit tests (Catch2)
  integration/    # Integration tests (full pipeline runs)
```

### Numeric Constants

- Use the project-level `PI` from `globalConstants.hpp` — never re-define locally
- `PI` must be defined with full double precision: `3.141592653589793`
- Prefer `std::acos(-1.0)` if in doubt

### Logging

- Use `PLOG` from `plog.hpp` for structured logging
- Severity levels: `info`, `warning`, `error`
- Log to both console and file

### Issue Tracking

- When fixing an issue, document the issue and the fix in a markdown file in folder `local`.
- Name the file `local/issue-YYYYMMDD-<issue-summary>.md`.
- Use a short summary of the issue as the file name.
