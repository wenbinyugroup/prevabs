cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project( prevabs )

set( CMAKE_CXX_STANDARD 11 )

set( PREVABS_MAJOR_VERSION 0 )
set( PREVABS_MINOR_VERSION 6 )
set( PREVABS_VERSION "${PREVABS_MAJOR_VERSION}.${PREVABS_MINOR_VERSION}" )

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++0x")
endif()

set( PROJECTNAME "test" )

# Main includes first, then local test includes to allow test-specific overrides
include_directories( ${CMAKE_SOURCE_DIR}/../../include )
include_directories( ${CMAKE_SOURCE_DIR}/../../include/gmsh )
include_directories( ${CMAKE_SOURCE_DIR} )

set( SOURCES 
  PGeoClasses.cpp
  PDCELVertex.cpp
  geo.cpp
  unittest.cpp
)

add_executable( ${PROJECTNAME} ${SOURCES} )

set( SOURCES_MATREP
  ../../src/MaterialRepository.cpp
  PGeoClasses.cpp
  PDCELVertex.cpp
  geo.cpp
  MaterialRepositoryTest.cpp
)
add_executable( test_matrep ${SOURCES_MATREP} )

# GeometryRepository test excluded - local test headers conflict with main includes
# set( SOURCES_GEOREP
#   ../../src/GeometryRepository.cpp
#   GeometryRepositoryTest.cpp
# )
# add_executable( test_georep ${SOURCES_GEOREP} )
