cmake_minimum_required(VERSION 3.14)
project(prevabs_unit_tests)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TEST_SOURCES
  catch_amalgamated.cpp
  test_geo.cpp
  test_geo_offset.cpp
)

add_executable(test_geo ${TEST_SOURCES})

# catch_amalgamated.hpp and test helpers live alongside the test sources;
# nlohmann JSON headers are in <repo_root>/include/
target_include_directories(test_geo PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/../../include)

target_compile_definitions(test_geo PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

enable_testing()
add_test(NAME geo_tests COMMAND test_geo)
