cmake_minimum_required(VERSION 3.8)

###############################################################################
## Set global configuration
###############################################################################

project(gbits)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

###############################################################################
# GBits configuration
###############################################################################

# These values configure where GBits parts are located or should be put. They
# can be defined by a game project to be wherever is convenient for building 
# into their cmake projects.

# Directory to the root of the gbits repository. If not defined, it is assumed
# this is the gbits repository or a copy/clone of it.
if (NOT DEFINED GBITS_DIR)
  set(GBITS_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

# Directory to the third party libraries gbits depends on. By default, this is
# points to gbits own third_party directory, but it can be redirected to any
# third party directory that has the required libraries.
if (NOT DEFINED GBITS_THIRD_PARTY_DIR)
  set(GBITS_THIRD_PARTY_DIR "${GBITS_DIR}/third_party")
endif()

# Directory to where all build output should go. This is where all cmake output
# is put for gbits, and code that uses the the gbits target utility functions.
if (NOT DEFINED GBITS_BUILD_DIR)
  set(GBITS_BUILD_DIR "${GBITS_DIR}/build")
endif()

# Directory to where all final executable output will go.
if (NOT DEFINED GBITS_BIN_DIR)
  set(GBITS_BIN_DIR "${GBITS_DIR}/bin")
endif()

###############################################################################
# GBits initialization
###############################################################################

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${GBITS_BIN_DIR}")
enable_testing()
add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

list(APPEND CMAKE_MODULE_PATH
  ${GBITS_DIR}/CMake
)
include(GBitsTargetCommands)

####### absl

# Disable Testing for abseil
set(BUILD_TESTING OFF) 
add_subdirectory("${GBITS_THIRD_PARTY_DIR}/abseil-cpp")
set(BUILD_TESTING ON) 

####### SDL2

add_library(SDL2 SHARED IMPORTED)
target_include_directories(SDL2 INTERFACE 
  "${GBITS_THIRD_PARTY_DIR}/SDL2-2.0.10/include"
)
set_target_properties(SDL2 PROPERTIES
  IMPORTED_IMPLIB
    "${GBITS_THIRD_PARTY_DIR}/SDL2-2.0.10/lib/x64/SDL2.lib"
)

add_library(SDL2_main STATIC IMPORTED)
set_target_properties(SDL2_main PROPERTIES
  IMPORTED_LOCATION
    "${GBITS_THIRD_PARTY_DIR}/SDL2-2.0.10/lib/x64/SDL2main.lib"
)

####### SDL2_image

add_library(SDL2_image SHARED IMPORTED)
target_include_directories(SDL2_image INTERFACE
  "${GBITS_THIRD_PARTY_DIR}/SDL2_image-2.0.5/include"
)
set_target_properties(SDL2_image PROPERTIES
  IMPORTED_IMPLIB
    "${GBITS_THIRD_PARTY_DIR}/SDL2_image-2.0.5/lib/x64/SDL2_image.lib"
)

####### glog

add_library(glog STATIC IMPORTED)
target_include_directories(glog INTERFACE
  "${GBITS_BUILD_DIR}/third_party/glog"
  "${GBITS_THIRD_PARTY_DIR}/glog/src/windows"
)
target_compile_definitions(glog INTERFACE
  # This is required as glog is built as a static library. 
  GOOGLE_GLOG_DLL_DECL=
  # This is required to be compatable with includes of windows.h
  GLOG_NO_ABBREVIATED_SEVERITIES=
)
set_target_properties(glog PROPERTIES
  IMPORTED_LOCATION
    "${GBITS_BUILD_DIR}/third_party/glog/$(Configuration)/glog.lib" 
  IMPORTED_LOCATION_DEBUG
    "${GBITS_BUILD_DIR}/third_party/glog/$(Configuration)/glogd.lib" 
)

###############################################################################
## Add gbits source
###############################################################################

add_subdirectory(src)
