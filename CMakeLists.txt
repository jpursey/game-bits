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

###############################################################################
# Game Bits configuration
###############################################################################

include(CMake/GameBitsConfig.cmake)
gb_config()

###############################################################################
# Game Bits initialization
###############################################################################

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${GB_BIN_DIR}")
enable_testing()
add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

list(APPEND CMAKE_MODULE_PATH
  "${GB_DIR}/CMake"
)
include(GameBitsTargetCommands)
include(GameBitsExternalLibraries)
gb_external_libraries()

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
