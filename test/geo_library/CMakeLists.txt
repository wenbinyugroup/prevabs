cmake_minimum_required(VERSION 3.15)
project(geo_test C CXX)

set(CMAKE_CXX_STANDARD 17)

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
find_library(GMSH_LIB gmsh PATH $ENV{Gmsh_ROOT}/lib)
if(NOT GMSH_LIB)
message(FATAL_ERROR "Could not find libgmsh")
endif()
message( STATUS "GMSH_LIB = " ${GMSH_LIB} )

# find_path(GMSH_INC gmsh.h PATH $ENV{Gmsh_ROOT}/include)
# if(NOT GMSH_INC)
# message(FATAL_ERROR "Could not find gmsh.h")
# endif()

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

target_include_directories(test PRIVATE $ENV{Clipper2_ROOT}/include)
target_include_directories(test PRIVATE $ENV{Gmsh_ROOT}/include)

target_link_libraries(test PRIVATE ${CLIPPER2_LIB} ${CLIPPER2_UTILS_LIB} ${GMSH_LIB} ${EXTRA_LIBS})
