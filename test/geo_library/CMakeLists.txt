cmake_minimum_required(VERSION 3.15)
project(geo_test C CXX)

set(CMAKE_CXX_STANDARD 17)
set(REPO_ROOT ${CMAKE_SOURCE_DIR}/../..)

# set(CLIPPER2_INC_FOLDER ${CMAKE_SOURCE_DIR}/../../include )
# message( STATUS "CLIPPER2_INC_FOLDER = " ${CLIPPER2_INC_FOLDER} )
# configure_file(clipper.version.in 
#   ${CLIPPER2_INC_FOLDER}/clipper.version.h NEWLINE_STYLE UNIX)

# include_directories( ${CMAKE_SOURCE_DIR}/../../include/clipper2 )
# link_directories(${CMAKE_PREFIX_PATH}/lib)

set(exp_src main.cpp)

# clipper2
find_library(CLIPPER2_LIB clipper2 PATHS $ENV{Clipper2_ROOT}/lib REQUIRED)
if(NOT CLIPPER2_LIB)
message(FATAL_ERROR "Could not find clipper2 library")
endif()
message( STATUS "CLIPPER2_LIB = " ${CLIPPER2_LIB} )

find_library(CLIPPER2_UTILS_LIB clipper2utils PATHS $ENV{Clipper2_ROOT}/lib REQUIRED)
if(NOT CLIPPER2_UTILS_LIB)
message(FATAL_ERROR "Could not find clipper2 library")
endif()
message( STATUS "CLIPPER2_UTILS_LIB = " ${CLIPPER2_UTILS_LIB} )

# gmsh
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
message( STATUS "GMSH_LIB = " ${GMSH_LIB} )

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

add_executable(test ${exp_src})

target_include_directories(test PRIVATE ${REPO_ROOT}/include)
target_include_directories(test PRIVATE $ENV{Clipper2_ROOT}/include)
target_include_directories(test PRIVATE ${GMSH_INC})

target_link_libraries(test PRIVATE ${CLIPPER2_LIB} ${CLIPPER2_UTILS_LIB} ${GMSH_LIB} ${EXTRA_LIBS})
