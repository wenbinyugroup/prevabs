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

set(CMAKE_CXX_STANDARD 11)
set(REPO_ROOT ${CMAKE_SOURCE_DIR}/../../..)

find_path(GMSH_INC
  NAMES gmsh.h gmsh.h_cwrap
  PATHS $ENV{Gmsh_ROOT}/include
  REQUIRED)
if(NOT GMSH_INC)
  message(FATAL_ERROR "Could not find gmsh headers")
endif()

if(WIN32 OR CYGWIN OR MINGW)
  set(GMSH_LIB "$ENV{Gmsh_ROOT}/lib/gmsh.lib")
  if(NOT EXISTS "${GMSH_LIB}")
    message(FATAL_ERROR "Could not find gmsh C API lib")
  endif()
else()
  find_library(GMSH_LIB gmsh PATHS $ENV{Gmsh_ROOT}/lib REQUIRED)
  if(NOT GMSH_LIB)
    message(FATAL_ERROR "Could not find libgmsh")
  endif()
endif()

if(GMSH_LIB MATCHES ".a") # FIXME - generalize this
  find_library(BLAS_LIB blas)
  if(BLAS_LIB)
    list(APPEND EXTRA_LIBS ${BLAS_LIB})
  endif()
  find_library(LAPACK_LIB lapack)
  if(LAPACK_LIB)
    list(APPEND EXTRA_LIBS ${LAPACK_LIB})
  endif()
endif()

if(WIN32 OR CYGWIN)
  list(APPEND EXTRA_LIBS winmm wsock32 ws2_32 psapi)
endif()

include_directories(${REPO_ROOT}/include)
include_directories(${GMSH_INC})

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
