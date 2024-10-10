cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project( prevabs C CXX )
# project( prevabs C CXX Fortran )

set( PROJECTNAME "prevabs" )

set( CMAKE_CXX_STANDARD 11 )

set( PREVABS_VERSION_MAJOR 1 )
set( PREVABS_VERSION_MINOR 6 )
set( PREVABS_VERSION_PATCH 0 )
set( PREVABS_VERSION "${PREVABS_VERSION_MAJOR}.${PREVABS_VERSION_MINOR}.${PREVABS_VERSION_PATCH}" )

# Generate version.h
configure_file(
  "${CMAKE_SOURCE_DIR}/version.h.in"
  "${CMAKE_BINARY_DIR}/generated/version.h"
  @ONLY
)

option(LINK_VABS_SHARED_LIB "Link VABS shared library" OFF)

# include(CheckCXXCompilerFlag)
# CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
# CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
# if(COMPILER_SUPPORTS_CXX11)
# 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
# elseif(COMPILER_SUPPORTS_CXX0X)
# 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
# else()
#   message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
# endif()



include_directories( ${CMAKE_SOURCE_DIR}/include )
# include_directories( ${CMAKE_BINARY_DIR}/generated )
# include_directories( ${CMAKE_SOURCE_DIR}/include/gmsh )

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")



if(MSVC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
endif()

add_executable( ${PROJECTNAME} ${SOURCES} )

# target_include_directories( ${PROJECTNAME} PUBLIC ${CMAKE_SOURCE_DIR}/include )
target_include_directories( ${PROJECTNAME} PUBLIC ${CMAKE_BINARY_DIR}/generated )


# Link Boost libraries
# --------------------
set(Boost_USE_STATIC_LIBS ON)
# set(Boost_USE_MULTITHREADED ON)
# set(Boost_USE_STATIC_RUNTIME OFF)
# add_definitions( -DBOOST_ALL_DYN_LINK )

find_package( Boost 1.66 COMPONENTS log log_setup system thread REQUIRED )
message( STATUS "(st) " ${Boost_INCLUDE_DIRS} )
message( STATUS "(st) " ${Boost_LIBRARY_DIRS} )
message( STATUS "(st) " ${Boost_LIBRARIES} )
include_directories( ${Boost_INCLUDE_DIRS} )

target_link_libraries( ${PROJECTNAME} ${Boost_LIBRARIES} )


# Link Gmsh library
# -----------------
message( STATUS "(st) " $ENV{Gmsh_ROOT} )

find_path(GMSH_INC gmsh.h PATHS $ENV{Gmsh_ROOT}/include REQUIRED)
if(NOT GMSH_INC)
  message(FATAL_ERROR "Could not find gmsh.h")
endif()
message( STATUS "(st) " ${GMSH_INC} )
include_directories(${GMSH_INC})

find_library(GMSH_LIB gmsh PATHS $ENV{Gmsh_ROOT}/lib REQUIRED)
if(NOT GMSH_LIB)
  message(FATAL_ERROR "Could not find libgmsh")
endif()
message( STATUS "(st) " ${GMSH_LIB} )
target_link_libraries( ${PROJECTNAME} ${GMSH_LIB} )

# find_library( GMSH NAMES Gmsh PATHS "./lib" REQUIRED NO_DEFAULT_PATH)
# message( STATUS "(st) " ${GMSH} )
# target_link_libraries( ${PROJECTNAME} ${GMSH} )


# Extra libraries
# ---------------
if(WIN32 OR CYGWIN)
  list(APPEND EXTRA_LIBS winmm wsock32 ws2_32 psapi)
  target_link_libraries( ${PROJECTNAME} ${EXTRA_LIBS} )
endif()


# Link VABS library
# -----------------
if ( LINK_VABS_SHARED_LIB )

  # find_library( VABS NAMES VABS PATHS "${VABS_INSTALL_DIR}" REQUIRED NO_DEFAULT_PATH)
  # find_library( VABS NAMES VABSLib.so PATHS "${VABS_INSTALL_DIR}" REQUIRED NO_DEFAULT_PATH )
  # message( ${VABS} )
  # set( CMAKE_SHARED_LIBRARY_PREFIX "" )

  # target_link_directories( ${PROJECTNAME} "/home/msg/a/MSGCodes" )
  # target_link_libraries( ${PROJECTNAME} ${VABS} )
  # target_link_libraries( ${PROJECTNAME} "${VABS_INSTALL_DIR}/VABSLib.so" )
  # target_link_options( ${PROJECTNAME} PRIVATE "${VABS_INSTALL_DIR}/VABSLib.so" )

  if( WIN32 OR CYGWIN OR MINGW )
    find_library( VABS NAMES VABSLIB.dll.a PATHS "${VABS_INSTALL_DIR}" REQUIRED NO_DEFAULT_PATH)
    message( STATUS "(st) " ${VABS} )
    target_link_libraries( ${PROJECTNAME} ${VABS} )

  elseif( UNIX )
    # find_library( VABS NAMES VABSLib.so PATHS "${VABS_INSTALL_DIR}" REQUIRED NO_DEFAULT_PATH)
    # message( ${VABS} )
    # target_link_libraries( ${PROJECTNAME} ${VABS} )
    # target_link_options( ${PROJECTNAME} PRIVATE "${VABS_INSTALL_DIR}/VABSLib.so" )
    # target_link_options( ${PROJECTNAME} PRIVATE "-l:VABSLib.so" )
    target_link_options( ${PROJECTNAME} PRIVATE "-L${VABS_INSTALL_DIR}" )
    target_link_options( ${PROJECTNAME} PRIVATE "-l:VABSLib.so" )

  endif()

endif()


# Install
# -------
install(TARGETS ${PROJECTNAME} RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/bin)
