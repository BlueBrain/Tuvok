# Minumum build script for building the Tuvok library needed for Volume Renderer

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(Tuvok)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake
  ${CMAKE_SOURCE_DIR}/CMake/common)

include(GitExternal)

set(VERSION_MAJOR "3")
set(VERSION_MINOR "1")
set(VERSION_PATCH "0")
set(VERSION_ABI 3) # OFF or ABI version

set(PROJECT_namespace tuvok)
set(PROJECT_INCLUDE_NAME Tuvok)
set(TUVOK_DESCRIPTION "Largescale Volume Data Access Library")
set(COMMON_PROJECT_DOMAIN ch.epfl.bluebrain)

include(Common)

common_package(BISON REQUIRED)
common_package(FLEX REQUIRED)
common_package(OpenGL REQUIRED)
common_package(OpenMP)
common_package(Qt5Core REQUIRED)
common_package(Qt5OpenGL REQUIRED)
common_package(ZLIB REQUIRED)
common_package_post()

include_directories( ${CMAKE_SOURCE_DIR}
                     Basics
                     Controller
                     DebugOut
                     Scripting
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

set( LINK_LIBRARIES pthread ${Qt5Widgets_LIBRARIES} ${ZLIB_LIBRARY}
                            ${OPENMP_LIBRARY} ${OPENGL_LIBRARY} )
if(APPLE)
  find_library(COREFOUNDATION_LIB CoreFoundation)
  list(APPEND LINK_LIBRARIES ${COREFOUNDATION_LIB})
endif()

if( ${CMAKE_COMPILER_IS_GNUCXX} )
  set(CMAKE_C_FLAGS "")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing" )
endif()

# Expressions library
BISON_TARGET( Parser IO/expressions/tvk-parse.ypp ${CMAKE_BINARY_DIR}/tvk-parse.cpp)
FLEX_TARGET( Scanner IO/expressions/tvk-scan.lpp ${CMAKE_BINARY_DIR}/tvk-scan.cpp)
ADD_FLEX_BISON_DEPENDENCY( Scanner Parser )

set( TUVOK_EXPRESSION_SOURCES  ${CMAKE_BINARY_DIR}/tvk-scan.cpp
                               ${CMAKE_BINARY_DIR}/tvk-parse.cpp
                               IO/VariantArray.cpp
                               IO/expressions/binary-expression.cpp
                               IO/expressions/conditional-expression.cpp
                               IO/expressions/constant.cpp
                               IO/expressions/treenode.cpp
                               IO/expressions/volume.cpp )

add_library( TuvokExpressions SHARED ${TUVOK_EXPRESSION_SOURCES} )
install( TARGETS TuvokExpressions LIBRARY DESTINATION lib )

# Tuvok library
include(files.cmake)

add_library(Tuvok SHARED ${TUVOK_SRCS})

include(InstallFiles)
install_files(include/Tuvok FILES ${TUVOK_HEADERS})

# TODO external packages should be explicit
target_link_libraries( Tuvok TuvokExpressions ${LINK_LIBRARIES} )

install( TARGETS Tuvok LIBRARY DESTINATION lib )
