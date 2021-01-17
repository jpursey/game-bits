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
enable_testing()
add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

list(APPEND CMAKE_MODULE_PATH
  "${GB_DIR}/CMake"
)
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
set(BUILD_TESTING) 

####### glm

add_subdirectory("${GB_THIRD_PARTY_DIR}/glm")

####### glog

option(WITH_GFLAGS "" OFF)
option(BUILD_TESTING "" OFF)
add_subdirectory("${GB_THIRD_PARTY_DIR}/glog")
set_target_properties(glog PROPERTIES FOLDER External/glog)

####### googletest

option(INSTALL_GTEST "" OFF)
option(gtest_force_shared_crt "" ON)
add_subdirectory("${GB_THIRD_PARTY_DIR}/googletest")
set_target_properties(gmock gmock_main gtest gtest_main PROPERTIES FOLDER External/googletest)

####### flatbuffers

set(FLATBUFFERS_FLATC_EXECUTABLE "${GB_DIR}/bin/flatc")
option(FLATBUFFERS_BUILD_TESTS "" OFF)
option(FLATBUFFERS_INSTALL "" OFF)
add_subdirectory("${GB_THIRD_PARTY_DIR}/flatbuffers")
set_target_properties(flatbuffers flatc flathash PROPERTIES FOLDER External/flatbuffers)

###############################################################################
## Add Game Bits source
###############################################################################

add_subdirectory(src)
