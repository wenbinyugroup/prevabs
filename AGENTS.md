# AGENTS.md

## Project Overview

PreVABS is a C++ preprocessing tool for VABS and SwiftComp. It builds cross-section geometry and generates input files for structural analysis.

- **Language**: C++11
- **Build System**: CMake (minimum 3.14)
- **Dependencies**: Gmsh SDK, git submodule `extern/cpp-terminal`
- **Test Framework**: [Catch2](https://github.com/catchorg/Catch2) (unit, single-header v3.x), CTest (integration)

## First Principles

Use first-principles thinking. Do not assume that I always clearly understand what I want or how to achieve it. Stay cautious and start from the fundamental needs and problems. If the motivation or goal is unclear, stop and discuss it with me.

---

## Build and Dependencies

### Source Dependency Bootstrap

PreVABS vendors `cpp-terminal` as the git submodule
`extern/cpp-terminal`. The top-level `CMakeLists.txt` builds it with
`add_subdirectory(extern/cpp-terminal)` and links
`cpp-terminal::cpp-terminal`, so there is no separate manual install step for
this dependency.

Before building from source, ensure submodules are present:

```bash
# Fresh clone
git clone --recursive <repo-url>

# Existing clone
git submodule update --init --recursive

# Refresh only cpp-terminal
git submodule update --init --recursive extern/cpp-terminal
```

After pulling new commits from the main repository, rerun
`git submodule update --init --recursive` so `extern/cpp-terminal` matches the
submodule commit pinned by PreVABS.

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
./tools/build_linux_64.sh

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

**Integration tests (CTest):**

Each test directory under `test/integration/` has an `INDEX.txt`:

```
main
- case1.xml
- case2.xml

support
- materials.xml
```

`main` lists input files to run; `support` lists files preserved during clean.
Every case runs `prevabs -i <xml> --hm` with a 15-second timeout.

Test names follow `<dir>/<case>`, e.g. `t1_strip/strip`.

```powershell
# Run all integration tests (from repo root)
pwsh -NoProfile -ExecutionPolicy Bypass -File test\run-integration-tests.ps1

# Run a subset — "t1" matches all t1_strip/* cases
pwsh -NoProfile -ExecutionPolicy Bypass -File test\run-integration-tests.ps1 -Filter t1

# Run one specific case (exact match via regex anchor)
pwsh -NoProfile -ExecutionPolicy Bypass -File test\run-integration-tests.ps1 -Filter "t1_strip/strip$"

# Clean generated outputs (keeps INDEX.txt and listed source files)
pwsh -NoProfile -ExecutionPolicy Bypass -File test\run-integration-tests.ps1 clean
pwsh -NoProfile -ExecutionPolicy Bypass -File test\run-integration-tests.ps1 clean -Filter t6
```

Or call CTest directly (after the script has run once to configure the build directory):

```powershell
ctest --test-dir test\integration\build_msvc --output-on-failure
ctest --test-dir test\integration\build_msvc -R t1        # filter by name regex
ctest --test-dir test\integration\build_msvc -L t1_strip  # filter by label
```

Adding a new test: create the directory, add `INDEX.txt` with the `main`/`support` sections,
and drop the XML input file in. No script changes needed — CMakeLists.txt picks it up automatically.

---

## Code Style Guidelines

- Encourage **modularity** and **reusability** in code design.
- Discourage **monolithic** code design.
- Avoid one giant function/file.
- When creating new functions, classes, modules, etc., start from simplest possible implementation, and gradually add features. **Do not overengineer**.
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

- Use `PLOG` from `plog.hpp` for **developer/debug** output: internal state,
  geometry details, source-location traces. Goes to `*.debug.log` and (when
  `--debug phase`+ is set) the console.
- Severity levels: `trace`, `debug`, `info`, `warning`, `error`, `fatal`
- For routine internal step logging prefer `PLOG(debug)` over `PLOG(info)` —
  `info` level should be reserved for messages a user would also benefit from.

### User UI

- Use `pui::*` from `pui.hpp` for **user-facing** terminal output: stage
  progress, success markers, user-level warnings/errors, banners.
- Functions: `pui::info` (`==>`), `pui::success` (`OK`), `pui::warn` (`!!`),
  `pui::error` (`xx`), `pui::title` (bold banner).
- Stream form: `PUI_INFO << "loading " << n << " points";`
- Output goes to stdout (with ANSI color when stdout is a TTY) and is
  mirrored to the user log file `*.log` without color codes.
- Rule of thumb:
  - "user wants to know what the program is doing" → `pui::info`
  - "developer wants to know internal state" → `PLOG(debug)`
- Do NOT call `pui::*` in tight loops or for fine-grained internal events;
  use `PLOG(debug)` instead.

### Issue Tracking

- When fixing an issue, document the issue and the fix in a markdown file in folder `local`.
- Name the file `local/issue-<yyyymmdd>-<short-summary>.md`.
