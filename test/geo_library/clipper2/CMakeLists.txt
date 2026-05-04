cmake_minimum_required(VERSION 3.15)
project(Clipper2 VERSION 1.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

# set(CLIPPER2_INC_FOLDER ${CMAKE_SOURCE_DIR}/../../include )
# message( STATUS "CLIPPER2_INC_FOLDER = " ${CLIPPER2_INC_FOLDER} )
# configure_file(clipper.version.in 
#   ${CLIPPER2_INC_FOLDER}/clipper.version.h NEWLINE_STYLE UNIX)

# include_directories( ${CMAKE_SOURCE_DIR}/../../include/clipper2 )
# link_directories(${CMAKE_PREFIX_PATH}/lib)

set(exp_src
  inflate.cpp
)

add_executable(test ${exp_src})

message( STATUS "ENV{Clipper2_ROOT} = " $ENV{Clipper2_ROOT}/include )
target_include_directories(test PRIVATE $ENV{Clipper2_ROOT}/include)

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
target_link_libraries(test PRIVATE ${CLIPPER2_LIB} ${CLIPPER2_UTILS_LIB})

