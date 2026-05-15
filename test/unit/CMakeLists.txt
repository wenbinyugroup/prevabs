cmake_minimum_required(VERSION 3.14)
project(prevabs_unit_tests)

set(CMAKE_CXX_STANDARD 17)
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
# cpp-terminal (used by pui, linked by test_dcel and test_pui)
# ------------------------------------------------------------------
set(CPPTERMINAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(CPPTERMINAL_ENABLE_DOCS    OFF CACHE BOOL "" FORCE)
add_subdirectory(${REPO_ROOT}/extern/cpp-terminal cpp-terminal-build)

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
# src/overloadOperator.cpp, and the CS/DCEL build sources required by the tests.
# ------------------------------------------------------------------
set(TEST_DCEL_SOURCES
  catch_amalgamated.cpp
  test_dcel.cpp
  test_io_airfoil.cpp
  test_io_airfoil_user_le_te.cpp
  test_io_component_laminate.cpp
  test_io_material.cpp
  test_io_p6_helpers.cpp
  test_main_pipeline.cpp
  test_io_baseline.cpp
  test_io_basepoint.cpp
  test_parse_helpers.cpp
  test_execu_timeout.cpp
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
  ${SRC_GEO}/CurveFrameLookup.cpp
  ${SRC_GEO}/geo_math.cpp
  ${SRC_GEO}/intersect.cpp
  ${SRC_GEO}/offset.cpp
  ${SRC_GEO}/PBST.cpp
  ${SRC_GEO}/polyline.cpp
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
  ${SRC_ROOT}/execu_utils.cpp
)

add_executable(test_dcel ${TEST_DCEL_SOURCES})

target_include_directories(test_dcel PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${REPO_ROOT}/include
  ${CMAKE_BINARY_DIR}/generated)

target_compile_definitions(test_dcel PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(test_dcel PRIVATE cpp-terminal::cpp-terminal)

if(MSVC)
  target_compile_options(test_dcel PRIVATE /W3 /utf-8 /EHa)
  target_compile_options(test_geo PRIVATE /W3 /utf-8 /EHa)
endif()

# ------------------------------------------------------------------
# test_curve_frame_lookup — standalone test for the polyline nearest-segment
# query introduced in plan-20260514-decouple-local-frame-from-map.md (Phase A).
# Only depends on header-only geo_types.hpp + linalg.h, so no project libs.
# ------------------------------------------------------------------
set(TEST_CFL_SOURCES
  catch_amalgamated.cpp
  test_curve_frame_lookup.cpp
  ${SRC_GEO}/CurveFrameLookup.cpp
)

add_executable(test_curve_frame_lookup ${TEST_CFL_SOURCES})

target_include_directories(test_curve_frame_lookup PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${REPO_ROOT}/include)

if(MSVC)
  target_compile_options(test_curve_frame_lookup PRIVATE /W3 /utf-8 /EHa)
  # M_PI etc. are not in <cmath> under MSVC without _USE_MATH_DEFINES.
  target_compile_definitions(test_curve_frame_lookup PRIVATE _USE_MATH_DEFINES)
endif()

# ------------------------------------------------------------------
# test_pui — pui module test, links cpp-terminal submodule.
# ------------------------------------------------------------------
set(TEST_PUI_SOURCES
  catch_amalgamated.cpp
  test_pui.cpp
  ${SRC_ROOT}/pui.cpp
)

add_executable(test_pui ${TEST_PUI_SOURCES})

target_include_directories(test_pui PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${REPO_ROOT}/include)

target_link_libraries(test_pui PRIVATE cpp-terminal::cpp-terminal)

if(MSVC)
  target_compile_options(test_pui PRIVATE /W3 /utf-8 /EHa)
endif()

# ------------------------------------------------------------------
# CTest registration
# ------------------------------------------------------------------
enable_testing()
add_test(NAME geo_tests                COMMAND test_geo)
add_test(NAME dcel_tests               COMMAND test_dcel)
add_test(NAME pui_tests                COMMAND test_pui)
add_test(NAME curve_frame_lookup_tests COMMAND test_curve_frame_lookup)
