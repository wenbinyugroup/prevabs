cmake_minimum_required(VERSION 3.15)
project(prevabs_clipper2_airfoil_tests LANGUAGES CXX)

# ------------------------------------------------------------------
# A0 of Clipper2 plan (plan-20260515-progress-review-and-next-steps.md
# §4.3.1): standalone airfoil offset tests for Clipper2.
#
# Hard constraint: zero PreVABS coupling. We only depend on:
#   - Clipper2 (via FetchContent at a fixed release tag)
#   - Catch2 amalgamated (copied from test/unit/)
#
# We deliberately do NOT pull in any PreVABS source files. If the
# PreVABS main source tree breaks tomorrow, this test executable
# must still build and run.
# ------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Clipper2 — pinned to the same tag the Clipper2 backend plan will use.
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

# Path to the Catch2 amalgamated single-header bundle living in
# test/unit/ (third-party file, not PreVABS code).
set(PREVABS_TEST_UNIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../unit")

# ------------------------------------------------------------------
# test_clipper2_airfoil — the actual test executable.
# ------------------------------------------------------------------
add_executable(test_clipper2_airfoil
  ${PREVABS_TEST_UNIT_DIR}/catch_amalgamated.cpp
  airfoil_loader.cpp
  geom_assertions.cpp
  svg_dump.cpp
  test_clipper2_airfoil.cpp
)

target_include_directories(test_clipper2_airfoil PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PREVABS_TEST_UNIT_DIR}
)

# AIRFOIL_DIR is consumed by airfoil_loader.cpp to locate the .dat
# files under test/geo_library/clipper2/airfoils/ regardless of where
# the test executable is run from.
target_compile_definitions(test_clipper2_airfoil PRIVATE
  CLIPPER2_AIRFOIL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/airfoils"
  CLIPPER2_AIRFOIL_OUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/svg_dumps"
)

if(MSVC)
  # MSVC's <cmath> only exposes M_PI when this macro is set before any
  # cmath/math.h include — airfoil_loader.cpp's NACA generator needs it.
  target_compile_definitions(test_clipper2_airfoil PRIVATE _USE_MATH_DEFINES)
endif()

# Clipper2 1.4.0 exposes the bare target name `Clipper2` (no namespace
# alias). If a future release adds `Clipper2::Clipper2`, prefer that.
target_link_libraries(test_clipper2_airfoil PRIVATE Clipper2)

if(MSVC)
  target_compile_options(test_clipper2_airfoil PRIVATE /W3 /utf-8 /EHa)
endif()

enable_testing()
add_test(NAME clipper2_airfoil_tests COMMAND test_clipper2_airfoil)

# The original evaluation skeleton (inflate.cpp / offset.cpp +
# build_msvc.bat) is preserved as historical reference under this
# directory but **not wired** into this CMakeLists.txt. It depends on
# the now-removed clipper2utils SVG helpers and uses an older Clipper2
# API. Anyone needing those can run build_msvc.bat directly against
# their own Clipper2 install.
