cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

# TODO: Gmsh should create a cmake file when installing the lib, so all the
# external dependencies can be obtained automatically

# if CMAKE_BUILD_TYPE is specified use it; otherwise set the default
# build type to "RelWithDebInfo" ("-O2 -g" with gcc) prior to calling
# project()
if(DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose build type")
else()
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose build type")
endif()

project(api_examples CXX C)

set(CMAKE_CXX_STANDARD 17)

include_directories("../../../include")

include(CTest)

file(GLOB EXAMPLES *.cpp)
foreach(EXAMPLE ${EXAMPLES})
  get_filename_component(EXAMPLENAME ${EXAMPLE} NAME_WE)
  # file(STRINGS ${EXAMPLE} EXAMPLE_CONTENTS NEWLINE_CONSUME)
  # string(REGEX MATCH "::fltk::" GUI ${EXAMPLE_CONTENTS})

  add_executable(${EXAMPLENAME} ${EXAMPLE})
  # add_test(${EXAMPLENAME}_cpp ${EXAMPLENAME} -nopopup)

  target_link_libraries(${EXAMPLENAME} ${GMSH_LIB} ${EXTRA_LIBS})
endforeach()
