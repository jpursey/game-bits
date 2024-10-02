## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

###############################################################################
## Set project configuration
###############################################################################

project(game-bits C CXX)

set(CMAKE_CXX_STANDARD 20)
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
add_compile_definitions(
  _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

list(APPEND CMAKE_MODULE_PATH
  "${GB_DIR}/CMake"
)
include(GameBitsTargetCommands)
include(GameBitsExternalLibraries)
gb_external_libraries()

###############################################################################
# Third party source
###############################################################################

# The following is necessary to prevent warnings that result in errors.
set(GB_3P_MSVC_COMPILE_OPTIONS 
  /wd4005  # L1: Macro redefinition
  /wd4068  # L1: Unknown pragma
  /wd4189  # L3: Local variable is initialized but not referenced
  /wd4244  # L3: Conversion from 'type1' to 'type2', possible loss of data
  /wd4267  # L3: Conversion from 'size_t' to 'type', possible loss of data
  /wd4800  # L3: Implicit conversion from 'type' to bool, possible loss of data
  /wd4100  # L4: Unreferenced formal parameter
  /wd4127  # L4: Consider using 'if constexpr' statement instead
  /wd4324  # L4: Structure was padded due to alignment specifier
)

# Turn CMake global variables BUILD_TESTING and BUILD_SHARED_LIBS explicitly off
# while building dependencies.

option(BUILD_TESTING "" ON)
set(BUILD_TESTING OFF)
option(BUILD_SHARED_LIBS "" OFF)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_SKIP_INSTALL_RULES TRUE)

####### absl

set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory("${GB_THIRD_PARTY_DIR}/abseil-cpp")

####### re2

add_subdirectory("${GB_THIRD_PARTY_DIR}/re2")
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(re2 PRIVATE ${GB_3P_MSVC_COMPILE_OPTIONS})
endif()

####### glm

add_subdirectory("${GB_THIRD_PARTY_DIR}/glm")

####### googletest

option(INSTALL_GTEST "" OFF)
set(INSTALL_GTEST OFF)
option(GTEST_HAS_ABSL "" ON)
set(GTEST_HAS_ABSL ON)
option(gtest_force_shared_crt "" ON)
set(gtest_force_shared_crt ON)
add_subdirectory("${GB_THIRD_PARTY_DIR}/googletest")
set_target_properties(gmock gmock_main gtest gtest_main PROPERTIES FOLDER External/googletest)
unset(INSTALL_GTEST)
unset(gtest_force_shared_crt)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(gtest PRIVATE ${GB_3P_MSVC_COMPILE_OPTIONS})
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(gmock PRIVATE ${GB_3P_MSVC_COMPILE_OPTIONS})
endif()

####### flatbuffers

set(FLATBUFFERS_FLATC_EXECUTABLE "${GB_DIR}/bin/flatc")
option(FLATBUFFERS_BUILD_TESTS "" OFF)
set(FLATBUFFERS_BUILD_TESTS OFF)
option(FLATBUFFERS_INSTALL "" OFF)
set(FLATBUFFERS_INSTALL OFF)
add_subdirectory("${GB_THIRD_PARTY_DIR}/flatbuffers")
set_target_properties(flatbuffers flatc PROPERTIES FOLDER External/flatbuffers)
unset(FLATBUFFERS_BUILD_TESTS)
unset(FLATBUFFERS_INSTALL)

####### libccd

add_subdirectory("${GB_THIRD_PARTY_DIR}/libccd")
set_target_properties(ccd PROPERTIES FOLDER External/ccd)
target_compile_definitions(ccd PUBLIC CCD_STATIC_DEFINE)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(ccd PRIVATE ${GB_3P_MSVC_COMPILE_OPTIONS})
endif()

###############################################################################
## Add Game Bits source
###############################################################################

add_subdirectory(src)
