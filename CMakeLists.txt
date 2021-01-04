## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

cmake_minimum_required(VERSION 3.8)

###############################################################################
## Set project configuration
###############################################################################

project(game-bits CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# This allows the <target>_FBS and <target>_TEST_FBS variables to work with
# local file paths.
cmake_policy(SET CMP0076 NEW)

###############################################################################
# Game Bits configuration
###############################################################################

include(CMake/GameBitsConfig.cmake)
gb_config()

###############################################################################
# Game Bits initialization
###############################################################################

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
enable_testing()
add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

list(APPEND CMAKE_MODULE_PATH
  "${GB_DIR}/CMake"
  "${GB_THIRD_PARTY_DIR}/flatbuffers/CMake"
)
set(FLATBUFFERS_FLATC_EXECUTABLE "${GB_THIRD_PARTY_BUILD_DIR}/flatbuffers/Release/flatc")
set(FLATBUFFERS_INCLUDE_DIR "${GB_THIRD_PARTY_DIR}/flatbuffers/include")
get_property(is_sub_project DIRECTORY PROPERTY PARENT_DIRECTORY)
if (is_sub_project)
  set(FLATBUFFERS_FLATC_EXECUTABLE "${FLATBUFFERS_FLATC_EXECUTABLE}" PARENT_SCOPE)
  set(FLATBUFFERS_INCLUDE_DIR "${FLATBUFFERS_INCLUDE_DIR}" PARENT_SCOPE)
endif()
include(BuildFlatBuffers)
include(GameBitsTargetCommands)
include(GameBitsExternalLibraries)
gb_external_libraries()

###############################################################################
# Third party source
###############################################################################

####### absl

# Disable Testing for abseil
set(BUILD_TESTING OFF) 
add_subdirectory("${GB_THIRD_PARTY_DIR}/abseil-cpp")
set(BUILD_TESTING ON) 

####### GLM

add_subdirectory("${GB_THIRD_PARTY_DIR}/glm")

###############################################################################
## Add Game Bits source
###############################################################################

add_subdirectory(src)
