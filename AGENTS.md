# AGENTS.md - PreVABS Development Guide

This file provides guidelines for agents working on the PreVABS codebase.

## Project Overview

PreVABS is a C++ preprocessing tool for VABS and SwiftComp. It builds cross-section geometry and generates input files for structural analysis.

- **Language**: C++11
- **Build System**: CMake (minimum 3.14)
- **Dependencies**: Gmsh SDK
- **Test Framework**: [Catch2](https://github.com/catchorg/Catch2) (single-header, v3.x)

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

```bash
# Build and run
cd test/unit
mkdir build && cd build
cmake ..
make
./test_geo
```

**Common Catch2 commands:**
```bash
./test_geo                      # Run all tests
./test_geo "[geo]"              # Run tests with tag [geo]
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

### General Rules

- **C++ Standard**: C++11 (set in CMakeLists.txt)
- **Indentation**: 2 spaces (no tabs)
- **Line endings**: Unix-style LF (configure your editor)
- **Max line length**: ~100 characters (soft guideline)
- **No commented-out dead code**: Remove dead code; git history preserves it

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase with 'P' prefix | `PModel`, `PDCELVertex`, `Baseline` |
| Structs | PascalCase | `LoadCase`, `LocalState` |
| Member variables | Underscore prefix (always) | `_name`, `_global_mesh_size` |
| Global variables | Lowercase with underscores | `debug`, `i_indent`, `config` |
| Functions/methods | PascalCase | `initialize()`, `buildGmsh()` |
| Constants | Lowercase with underscores | `single_line` |
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

### Header Files

- **Always** use `#pragma once` as the first line — no exceptions
- Never use `using namespace` at file scope in a header
- Order includes:
  1. Corresponding header (for .cpp files)
  2. Project headers (quotes)
  3. External/system headers (angle brackets)
- Include only what the file directly uses — do not rely on transitive includes

### Source File Template

```cpp
#pragma once  // (headers only)

#include "PModel.hpp"

#include "PModelIO.hpp"
#include "CrossSection.hpp"

#include <gmsh.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
```

### Code Formatting

- **Braces**: Always use braces, even for single-line statements
- **Spaces**: No space between function name and parenthesis
- **References**: Use `&` attached to type: `std::string& name`
- **Pointers**: Use `*` attached to type: `PElementNodeData* _u`

Example:

```cpp
void PModel::initialize() {
  if (config.debug) {
    config.fdeb = fopen(...);
  }
  gmsh::initialize();
}
```

### Error Handling

- Use `std::exception` and derived classes
- Wrap risky operations in try/catch blocks
- Print errors to `std::cerr` with `"ERROR: "` prefix
- Return early on failure conditions
- **Always** check return values of `fopen()`, `fclose()`, and similar C APIs

```cpp
try {
  pmodel->build();
} catch (std::exception& e) {
  std::cerr << "ERROR: " << e.what() << std::endl;
  return 1;  // non-zero exit on error
}
```

### XML Parsing (RapidXML)

- **Always** null-check the result of `first_node()` and `first_attribute()` before dereferencing
- Throw `std::runtime_error` with a descriptive message when a required element is missing

```cpp
xml_node<>* node = parent->first_node("required_tag");
if (!node) {
  throw std::runtime_error("Missing required XML element <required_tag>");
}
```

- Use `std::stod()` / `std::stoi()` (not `atof()` / `atoi()`) to parse numeric text; catch exceptions and report the offending field

### Numeric Constants

- Use the project-level `PI` from `globalConstants.hpp` — never re-define locally
- `PI` must be defined with full double precision: `3.141592653589793`
- Prefer `std::acos(-1.0)` if in doubt

### Logging

- Use `PLOG` from `plog.hpp` for structured logging
- Severity levels: `info`, `warning`, `error`
- Log to both console and file

### Memory Management

- **Prefer `std::unique_ptr` and `std::shared_ptr`** over raw owning pointers
- If a raw `new` is unavoidable, document ownership explicitly and ensure `delete` in the destructor
- Initialize all pointer members to `nullptr` in constructors
- Every `new` must have a corresponding `delete`; use RAII to guarantee this

```cpp
// Preferred
auto pmodel = std::make_unique<PModel>(config.file_base_name);

// Avoid
PModel* pmodel = new PModel(config.file_base_name);  // easy to leak
```

### External Process Execution

- **Never** use `std::system()` with user-controlled input — this is a shell injection risk
- On Windows use `CreateProcess`; on POSIX use `fork`/`execvp` with an argument array
- Sanitize or validate all file paths before use in commands

### Modern C++ Patterns

- Use `auto` for iterator types and complex template types
- Use range-based for loops: `for (auto& p : _vertices)`
- Use initializer lists for struct initialization
- Prefer `std::vector` over C arrays
- Use `std::string` instead of `char*`

### Performance Considerations

- Pass large objects by const reference: `const std::vector<double>&`
- Reserve vector capacity when size is known
- Avoid unnecessary copies in loops

---

## Project Architecture

### Key Classes

| Class | Purpose |
|-------|---------|
| `PModel` | Main model containing all data |
| `PDCEL` | Half-edge data structure for geometry |
| `Baseline` | Baseline curves |
| `PComponent` | Component definitions |
| `Material` | Material properties |
| `CrossSection` | Cross-section geometry |

### Processing Pipeline

1. **Initialize**: Setup Gmsh, open log file
2. **Read Inputs**: Parse XML input files
3. **Build**: Create geometry and mesh
4. **Write Output**: Generate SG input file for VABS/SwiftComp
5. **Execute**: Optionally run VABS/SwiftComp
6. **Visualize**: Optionally open Gmsh viewer
7. **Finalize**: Close files and cleanup

---

## Common Tasks

### Adding a New Class

1. Create `include/MyClass.hpp` — add `#pragma once` as the first line
2. Create `src/MyClass.cpp` with implementation
3. Add the `.cpp` file **explicitly** to `CMakeLists.txt` (do not rely on GLOB)

### Running a Test Case

```bash
# From build directory
./prevabs.exe -i ../test/box/input.xml --hm -v
```

### Writing Unit Tests

Each test file should include Catch2 and define tests:

```cpp
#define CATCH_CONFIG_MAIN  // Only ONE file should have this
#include "catch_amalgam.hpp"

TEST_CASE("description of what is being tested") {
  // Arrange
  int expected = 42;

  // Act
  int actual = calculate();

  // Assert
  CHECK(actual == expected);      // Continues on failure
  REQUIRE(actual == expected);    // Stops on failure
}

TEST_CASE("grouped tests", "[geometry]") {
  // Tagged tests can be run with: ./test "[geometry]"
}
```

**Common assertions:**
- `CHECK(expr)` - Continues even if failed
- `REQUIRE(expr)` - Stops on failure
- `CHECKED_IF(expr)` / `CHECKED_ELSE(expr)` - Branch coverage
- `REQUIRE_THROWS(expr)` - Expect exception
- `REQUIRE_THAT(val, Catch::Matchers::Equals(expected))` - Container comparison

### Debugging

Enable debug mode with `--debug` flag:
```bash
prevabs.exe -i input.xml --hm --debug
```

This creates a `.debug` file with detailed output.

### Issue Tracking

- When fixing an issue, document the issue and the fix in a markdown file in folder `local`.
- Name the file `local/issue-YYYYMMDD-<issue-summary>.md`.
- Use a short summary of the issue as the file name.

---

## External Dependencies

- **Gmsh**: Must have Gmsh SDK with `Gmsh_ROOT` environment variable set
- On Windows: Use MSVC developer command prompt for builds
- **CLI11**: Vendored in `include/CLI11.hpp` — migration from legacy `parseArguments()` is in progress
- **RapidXML**: Vendored in `include/rapidxml/` — header-only, no version tracking
- **Catch2**: Download single-header from releases; placed in `test/unit/`
