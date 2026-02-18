cmake_minimum_required(VERSION 3.14)
project(prevabs_unit_tests)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TEST_SOURCES
  test_geo.cpp
)

add_executable(test_geo ${TEST_SOURCES})

enable_testing()
add_test(NAME geo_tests COMMAND test_geo)
