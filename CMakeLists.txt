# Minumum build script for building the Tuvok library needed for Volume Renderer

cmake_minimum_required(VERSION 2.8)
project(Tuvok)

find_package( Qt4 REQUIRED QtCore QtGui QtXml QtOpenGL )
find_package( BISON REQUIRED )
find_package( FLEX REQUIRED )

include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
link_directories( /usr/lib )

option(SHARED_LIBRARY "Shared library" ON)

include_directories( ${CMAKE_SOURCE_DIR}
                     Basics
                     Controller
                     DebugOut
                     Renderer
                     Scripting
                     3rdParty/GLEW
                     IO
                     IO/UVF
                     IO/3rdParty/boost
                     IO/3rdParty/bzip2
                     IO/3rdParty/jpeglib
                     IO/3rdParty/lz4
                     IO/3rdParty/lzham
                     IO/3rdParty/lzma
                     IO/3rdParty/tiff
                     IO/3rdParty/zlib
                     IO/exception
                     IO/expressions
                     ${QT_INCLUDE_DIR} )

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if( ${CMAKE_COMPILER_IS_GNUCXX} )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fno-strict-aliasing -fopenmp" )
endif( ${CMAKE_COMPILER_IS_GNUCXX} )

# Expressions library
BISON_TARGET( Parser IO/expressions/tvk-parse.ypp ${CMAKE_BINARY_DIR}/tvk-parse.cpp)
FLEX_TARGET( Scanner IO/expressions/tvk-scan.lpp ${CMAKE_BINARY_DIR}/tvk-scan.cpp)
ADD_FLEX_BISON_DEPENDENCY( Scanner Parser )

set( TUVOK_EXPRESSION_SOURCES  ${CMAKE_BINARY_DIR}/tvk-scan.cpp
                               ${CMAKE_BINARY_DIR}/tvk-parse.cpp
                               IO/expressions/binary-expression.cpp
                               IO/expressions/conditional-expression.cpp
                               IO/expressions/constant.cpp
                               IO/expressions/treenode.cpp
                               IO/expressions/volume.cpp )


add_library( TuvokExpressions SHARED ${TUVOK_EXPRESSION_SOURCES} )
install( TARGETS TuvokExpressions LIBRARY DESTINATION lib )

# Tuvok library

include(files.cmake)

if( SHARED_LIBRARY )
   add_library( Tuvok SHARED ${TUVOK_SRCS} )
else( SHARED_LIBRARY )
   add_library( Tuvok ${TUVOK_SRCS})
endif( SHARED_LIBRARY )

macro(INSTALL_HEADERS_WITH_DIRECTORY HEADER_LIST)
   foreach(HEADER ${${HEADER_LIST}})
       string(REGEX MATCH "(Tuvok/.*)[/\\]" DIR ${HEADER})
       if("${DIR}" STREQUAL "")
          install(FILES ${HEADER} DESTINATION include/Tuvok)
       else("${DIR}" STREQUAL "")
          install(FILES ${HEADER} DESTINATION include/${DIR})
       endif("${DIR}" STREQUAL "")
   endforeach(HEADER)
ENDMACRO(INSTALL_HEADERS_WITH_DIRECTORY)

INSTALL_HEADERS_WITH_DIRECTORY( TUVOK_HEADERS )

# TODO external packages should be explicit
target_link_libraries( Tuvok TuvokExpressions ${QT_LIBRARIES} ${ZLIB_LIBRARY} z pthread  GL GLU gomp )

SET_TARGET_PROPERTIES( Tuvok PROPERTIES LINK_FLAGS "-Wl,--no-undefined" )
install( TARGETS Tuvok LIBRARY DESTINATION lib )


