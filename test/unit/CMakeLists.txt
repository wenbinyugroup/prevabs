cmake_minimum_required(VERSION 3.14)
project(prevabs_unit_tests)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT ${CMAKE_SOURCE_DIR}/../..)

# Generate version.h needed by utilities.cpp
set(PREVABS_VERSION_MAJOR 1)
set(PREVABS_VERSION_MINOR 6)
set(PREVABS_VERSION_PATCH 0)
set(PREVABS_VERSION "${PREVABS_VERSION_MAJOR}.${PREVABS_VERSION_MINOR}.${PREVABS_VERSION_PATCH}")
configure_file(
  "${REPO_ROOT}/version.h.in"
  "${CMAKE_BINARY_DIR}/generated/version.h"
  @ONLY
)
set(SRC_GEO  ${REPO_ROOT}/src/geo)
set(SRC_ROOT ${REPO_ROOT}/src)

# ------------------------------------------------------------------
# test_geo — self-contained geometry algorithm tests (no project deps)
# ------------------------------------------------------------------
set(TEST_GEO_SOURCES
  catch_amalgamated.cpp
  test_geo.cpp
  test_geo_offset.cpp
)

add_executable(test_geo ${TEST_GEO_SOURCES})

target_include_directories(test_geo PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${REPO_ROOT}/include)

target_compile_definitions(test_geo PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# ------------------------------------------------------------------
# test_dcel — DCEL topology tests compiled against the real sources.
# No Gmsh required: only src/geo/, src/plog.cpp, src/utilities.cpp,
# src/overloadOperator.cpp, and src/cs/PBaseLine.cpp are included.
# ------------------------------------------------------------------
set(TEST_DCEL_SOURCES
  catch_amalgamated.cpp
  test_dcel.cpp
  ${SRC_GEO}/DCELSweepLine.cpp
  ${SRC_GEO}/DCELValidator.cpp
  ${SRC_GEO}/PDCEL.cpp
  ${SRC_GEO}/PDCELFace.cpp
  ${SRC_GEO}/PDCELHalfEdge.cpp
  ${SRC_GEO}/PDCELHalfEdgeLoop.cpp
  ${SRC_GEO}/PDCELVertex.cpp
  ${SRC_GEO}/PGeoClasses.cpp
  ${SRC_GEO}/PGeoLine.cpp
  ${SRC_GEO}/curve.cpp
  ${SRC_GEO}/geo_math.cpp
  ${SRC_GEO}/intersect.cpp
  ${SRC_GEO}/offset.cpp
  ${SRC_GEO}/PBST.cpp
  ${SRC_GEO}/polyline.cpp
  ${SRC_ROOT}/plog.cpp
  ${SRC_ROOT}/utilities.cpp
  ${SRC_ROOT}/overloadOperator.cpp
  ${SRC_ROOT}/GeometryRepository.cpp
  ${SRC_ROOT}/MaterialRepository.cpp
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
)

add_executable(test_dcel ${TEST_DCEL_SOURCES})

target_include_directories(test_dcel PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${REPO_ROOT}/include
  ${CMAKE_BINARY_DIR}/generated)

target_compile_definitions(test_dcel PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

if(MSVC)
  target_compile_options(test_dcel PRIVATE /W3 /utf-8)
endif()

# ------------------------------------------------------------------
# CTest registration
# ------------------------------------------------------------------
enable_testing()
add_test(NAME geo_tests  COMMAND test_geo)
add_test(NAME dcel_tests COMMAND test_dcel)
