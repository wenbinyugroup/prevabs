cmake_minimum_required(VERSION 3.15)
project(t0_offset_clipper2 LANGUAGES CXX)

# ------------------------------------------------------------------
# Standalone harness for iterating on Clipper2-offset →
# staircase-base-offset-map construction.
#
# Coupling principle (matches test/geo_library/clipper2/ pattern):
#   - link **only** the two pure-logic PreVABS source files
#       src/geo/offset_clipper2.cpp        (Stage B Clipper2 wrapper)
#       src/geo/offset_clipper2_bridge.cpp (Stage C + 3 matchers + Stage E
#                                           pre-trim helpers)
#   - + Clipper2 (FetchContent, same pin as main build)
#   - **NO** PDCELVertex / PSegment / PModel / gmsh / spdlog / cpp-terminal
#
# If the main PreVABS source tree breaks, this harness must still build
# and run as long as `offset_clipper2.hpp` + `geo_types.hpp` keep their
# current contracts.
# ------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

include(FetchContent)

set(CLIPPER2_TESTS    OFF CACHE BOOL "" FORCE)
set(CLIPPER2_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLIPPER2_UTILS    OFF CACHE BOOL "" FORCE)
set(CLIPPER2_USINGZ   OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  clipper2
  GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
  GIT_TAG        Clipper2_1.4.0
  SOURCE_SUBDIR  CPP
)
FetchContent_MakeAvailable(clipper2)

add_executable(test_offset_map
  test_offset_map.cpp
  ${REPO_ROOT}/src/geo/offset_clipper2.cpp
  ${REPO_ROOT}/src/geo/offset_clipper2_bridge.cpp
)

target_include_directories(test_offset_map PRIVATE
  ${REPO_ROOT}/include
)

target_compile_definitions(test_offset_map PRIVATE
  T0_OUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/out"
  T0_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(test_offset_map PRIVATE Clipper2)

if(MSVC)
  target_compile_options(test_offset_map PRIVATE /W3 /utf-8 /EHa)
endif()

# ------------------------------------------------------------------
# phase0_layered_v — Phase-0 prototype for
# plan-20260618-per-layer-offset-within-shell.md. Same coupling
# principle: links only the two pure-logic geo TUs + Clipper2.
# ------------------------------------------------------------------
add_executable(phase0_layered_v
  phase0_layered_v.cpp
  ${REPO_ROOT}/src/geo/offset_clipper2.cpp
  ${REPO_ROOT}/src/geo/offset_clipper2_bridge.cpp
)

target_include_directories(phase0_layered_v PRIVATE
  ${REPO_ROOT}/include
)

target_compile_definitions(phase0_layered_v PRIVATE
  PHASE0_OUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/out"
)

target_link_libraries(phase0_layered_v PRIVATE Clipper2)

if(MSVC)
  target_compile_options(phase0_layered_v PRIVATE /W3 /utf-8 /EHa)
endif()

# ------------------------------------------------------------------
# compare_resample — old foot-resample vs new raw-miter offset (SVG).
# Same coupling: offset core + Clipper2.
# ------------------------------------------------------------------
add_executable(compare_resample
  compare_resample.cpp
  ${REPO_ROOT}/src/geo/offset_clipper2.cpp
  ${REPO_ROOT}/src/geo/offset_clipper2_bridge.cpp
)

target_include_directories(compare_resample PRIVATE
  ${REPO_ROOT}/include
)

target_compile_definitions(compare_resample PRIVATE
  PHASE0_OUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/out"
)

target_link_libraries(compare_resample PRIVATE Clipper2)

if(MSVC)
  target_compile_options(compare_resample PRIVATE /W3 /utf-8 /EHa)
endif()

# Make the output directory at configure time so the executable can
# always write into it.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/out")
