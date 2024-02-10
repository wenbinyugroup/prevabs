cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project( prevabs )
# set( PROJECTNAME "prevabs" )


set( CMAKE_CXX_STANDARD 11 )

set( PREVABS_MAJOR_VERSION 0 )
set( PREVABS_MINOR_VERSION 6 )
set( PREVABS_VERSION "${PREVABS_MAJOR_VERSION}.${PREVABS_MINOR_VERSION}" )

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
  message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)
# message( ${CMAKE_BINARY_DIR} )

# find_library( GMSH NAMES Gmsh PATHS "../lib" REQUIRED NO_DEFAULT_PATH)
# message( ${GMSH} )

# add_subdirectory(gmsh)

# include_directories( ${CMAKE_SOURCE_DIR}/include )
# include_directories( ${CMAKE_SOURCE_DIR}/include/gmsh )

set( PROJECTNAME "test" )
set( SOURCES 
  PGeoClasses.cpp
  PDCELVertex.cpp
  geo.cpp
  unittest.cpp
)

add_executable( ${PROJECTNAME} ${SOURCES} )
# target_link_libraries( ${PROJECTNAME} ${GMSH} )
# target_link_libraries( ${PROJECTNAME} Gmsh )

install(TARGETS ${PROJECTNAME} RUNTIME DESTINATION bin)
