cmake_minimum_required(VERSION 3.5.0)
project(prevabs VERSION 1.4.0 LANGUAGES C CXX)


set( CMAKE_CXX_STANDARD 11 )

include_directories( ${CMAKE_SOURCE_DIR}/include )

set( PROJECTNAME "prevabs" )
set( SOURCES 
  src/geo/PGeoClasses.cpp
  src/geo/PGeoLine.cpp
  src/geo/PDCELVertex.cpp
  src/geo/PDCELHalfEdge.cpp
  src/geo/PDCELHalfEdgeLoop.cpp
  src/geo/PDCELFace.cpp
  src/geo/PDCEL.cpp
  src/geo/PBST.cpp
  src/geo/geo.cpp
  src/geo/offset.cpp
  src/geo/intersect.cpp
  src/cs/PArea.cpp
  src/cs/Material.cpp
  src/cs/PSegment.cpp
  src/cs/PComponent.cpp
  src/cs/PBuildComponentLaminate.cpp
  src/cs/PBuildComponentFilling.cpp
  src/cs/PBuildSegmentAreas.cpp
  src/cs/CrossSection.cpp
  src/cs/PBaseLine.cpp
  src/cs/PModel.cpp
  src/cs/join.cpp
  src/io/PModelIO.cpp
  src/io/PModelIOReadBasePoint.cpp
  src/io/PModelIOReadBaseLine.cpp
  src/io/PModelIOReadMaterial.cpp
  src/io/PModelIOReadLayup.cpp
  src/io/PModelIOReadComponent.cpp
  src/io/PModelIOReadComponentLaminate.cpp
  src/io/PModelIOReadComponentFilling.cpp
  src/io/PModelIOReadCrossSection.cpp
  src/io/PModelIODehomo.cpp
  src/io/PModelIOSG.cpp
  src/io/PModelIOSupplement.cpp
  src/io/PModelIOPlot.cpp
  src/gmsh/PModelBuildGmsh.cpp
  src/gmsh/PModelIOGmsh.cpp
  src/plog.cpp
  src/overloadOperator.cpp
  src/utilities.cpp
  src/execu.cpp
  src/main.cpp
  # src/vabs/output.f90
)

add_executable( ${PROJECTNAME} ${SOURCES} )


# Link Boost libraries
# --------------------
set(Boost_USE_STATIC_LIBS OFF)
# set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
add_definitions( -DBOOST_ALL_DYN_LINK )

find_package( Boost 1.85 COMPONENTS log log_setup system thread REQUIRED )
message( STATUS ${Boost_INCLUDE_DIRS} )
message( STATUS ${Boost_LIBRARY_DIRS} )
message( STATUS ${Boost_LIBRARIES} )
include_directories( ${Boost_INCLUDE_DIRS} )

target_link_libraries( ${PROJECTNAME} ${Boost_LIBRARIES} )




# Link Gmsh library
# -----------------
message( STATUS ${Gmsh_DIR} )
find_library( GMSH NAMES gmsh HINTS Gmsh_DIR REQUIRED )
message( STATUS ${GMSH} )

target_link_libraries( ${PROJECTNAME} ${GMSH} )



# Link VABS library
# -----------------
message( STATUS ${VABS_DIR} )

if( WIN32 OR CYGWIN OR MINGW )
  find_library( VABS NAMES VABSLIB HINTS "${VABS_DIR}" REQUIRED )
  message( STATUS ${VABS} )
  target_link_libraries( ${PROJECTNAME} ${VABS} )

elseif( UNIX )
  # find_library( VABS NAMES VABSLib.so PATHS "${VABS_INSTALL_DIR}" REQUIRED NO_DEFAULT_PATH)
  # message( ${VABS} )
  # target_link_libraries( ${PROJECTNAME} ${VABS} )
  # target_link_options( ${PROJECTNAME} PRIVATE "${VABS_INSTALL_DIR}/VABSLib.so" )
  # target_link_options( ${PROJECTNAME} PRIVATE "-l:VABSLib.so" )
  target_link_options( ${PROJECTNAME} PRIVATE "-L${VABS_DIR}" )
  target_link_options( ${PROJECTNAME} PRIVATE "-l:VABSLib.so" )

endif()


# Install
# -------
install(TARGETS ${PROJECTNAME} RUNTIME DESTINATION bin)

