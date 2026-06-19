cmake_minimum_required(VERSION 3.15)
project(t0_dcel LANGUAGES CXX)

# ------------------------------------------------------------------
# Standalone harness for PDCEL::splitFaceByPolyline (and the repeated-split
# band-subdivision pattern). Emits a DCEL dump + SVG per case.
#
# Source closure mirrors the unit `test_dcel` target: PDCEL pulls the geo +
# utilities closure, which is header-coupled to cs/Material. This is the same
# proven closure the DCEL unit tests link against — no Gmsh required.
# ------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(SRC_GEO  "${REPO_ROOT}/src/geo")
set(SRC_ROOT "${REPO_ROOT}/src")

# version.h (needed transitively by utilities.cpp closure)
set(PREVABS_VERSION_MAJOR 1)
set(PREVABS_VERSION_MINOR 6)
set(PREVABS_VERSION_PATCH 0)
set(PREVABS_VERSION "${PREVABS_VERSION_MAJOR}.${PREVABS_VERSION_MINOR}.${PREVABS_VERSION_PATCH}")
configure_file(
  "${REPO_ROOT}/version.h.in"
  "${CMAKE_BINARY_DIR}/generated/version.h"
  @ONLY)

# cpp-terminal (used by pui, transitively linked)
set(CPPTERMINAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_DOCS    OFF CACHE BOOL "" FORCE)
add_subdirectory(${REPO_ROOT}/extern/cpp-terminal cpp-terminal-build)

# Clipper2 (FetchContent, same pin as the main build)
include(FetchContent)
set(CLIPPER2_TESTS    OFF CACHE BOOL "" FORCE)
set(CLIPPER2_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLIPPER2_UTILS    OFF CACHE BOOL "" FORCE)
set(CLIPPER2_USINGZ   OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  clipper2
  GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
  GIT_TAG        Clipper2_1.4.0
  SOURCE_SUBDIR  CPP)
FetchContent_MakeAvailable(clipper2)

add_executable(t0_dcel
  t0_dcel.cpp
  ${SRC_GEO}/DCELSweepLine.cpp
  ${SRC_GEO}/DCELValidator.cpp
  ${SRC_GEO}/PDCEL.cpp
  ${SRC_GEO}/PDCELFace.cpp
  ${SRC_GEO}/PDCELHalfEdge.cpp
  ${SRC_GEO}/PDCELHalfEdgeLoop.cpp
  ${SRC_GEO}/PDCELVertex.cpp
  ${SRC_GEO}/PGeoClasses.cpp
  ${SRC_GEO}/PGeoLine.cpp
  ${SRC_GEO}/adaptive_thickness.cpp
  ${SRC_GEO}/curve.cpp
  ${SRC_GEO}/CurveFrameLookup.cpp
  ${SRC_GEO}/geo_math.cpp
  ${SRC_GEO}/intersect.cpp
  ${SRC_GEO}/offset.cpp
  ${SRC_GEO}/offset_clipper2.cpp
  ${SRC_GEO}/offset_clipper2_bridge.cpp
  ${SRC_GEO}/offset_clipper2_pdcel.cpp
  ${SRC_GEO}/PBST.cpp
  ${SRC_GEO}/polyline.cpp
  ${SRC_GEO}/variable_offset.cpp
  ${SRC_ROOT}/plog.cpp
  ${SRC_ROOT}/pui.cpp
  ${SRC_ROOT}/utilities.cpp
  ${SRC_ROOT}/overloadOperator.cpp
  ${SRC_ROOT}/GeometryRepository.cpp
  ${SRC_ROOT}/MaterialRepository.cpp
  ${SRC_ROOT}/io/PModelIOReadAirfoil.cpp
  ${SRC_ROOT}/io/PModelIOReadBaseLine.cpp
  ${SRC_ROOT}/io/PModelIOReadBasePoint.cpp
  ${SRC_ROOT}/io/PModelIOReadComponent.cpp
  ${SRC_ROOT}/io/PModelIOReadComponentFilling.cpp
  ${SRC_ROOT}/io/PModelIOReadComponentLaminate.cpp
  ${SRC_ROOT}/io/PModelIOReadLayup.cpp
  ${SRC_ROOT}/io/PModelIOReadMaterial.cpp
  ${SRC_ROOT}/cs/CrossSection.cpp
  ${SRC_ROOT}/cs/Material.cpp
  ${SRC_ROOT}/cs/PArea.cpp
  ${SRC_ROOT}/cs/PBaseLine.cpp
  ${SRC_ROOT}/cs/PComponent.cpp
  ${SRC_ROOT}/cs/PBuildComponentFilling.cpp
  ${SRC_ROOT}/cs/PBuildComponentLaminate.cpp
  ${SRC_ROOT}/cs/PBuildSegmentAreas.cpp
  ${SRC_ROOT}/cs/PFilling.cpp
  ${SRC_ROOT}/cs/PSegment.cpp
  ${SRC_ROOT}/cs/join.cpp
  ${SRC_ROOT}/debug/baseOffsetMapSvg.cpp
  ${SRC_ROOT}/debug/baseOffsetMapJson.cpp
  ${SRC_ROOT}/execu_utils.cpp)

target_include_directories(t0_dcel PRIVATE
  ${REPO_ROOT}/include
  ${CMAKE_BINARY_DIR}/generated)

target_compile_definitions(t0_dcel PRIVATE
  T0_DCEL_OUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/out")

target_link_libraries(t0_dcel PRIVATE cpp-terminal::cpp-terminal Clipper2)

if(MSVC)
  target_compile_options(t0_dcel PRIVATE /W3 /utf-8 /EHa)
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/out")
