## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
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

# CMake's RelWithDebInfo inlines only functions marked inline (/Ob1), which
# leaves the standard library's internal templates as calls, such as the two
# inside every std::atomic load and store. This inlines as Release does. It is
# set in the cache, so it applies to the whole build, including the project
# that adds Game Bits as a subdirectory.
if (MSVC)
  foreach(_lang C CXX)
    string(REPLACE "/Ob1" "/Ob2" _flags "${CMAKE_${_lang}_FLAGS_RELWITHDEBINFO}")
    set(CMAKE_${_lang}_FLAGS_RELWITHDEBINFO "${_flags}" CACHE STRING
        "Flags used by the ${_lang} compiler during RELWITHDEBINFO builds."
        FORCE)
  endforeach()
endif()

# A web build (emcmake cmake) is either without threads, the default, or with
# them. With threads, everything in Game Bits, third-party code included, is
# compiled and linked with -pthread, as code compiled without it can't link
# with code compiled with it. Game Bits' libraries pass it on, so a project's
# own targets that use them get it too (see gb_set_target_properties). Any
# other target in a project needs it added by the project.
#
# Abseil's headers use Emscripten's old version macros, which Emscripten 7
# deprecates, so -Wno-deprecated-pragma is on for everything built here too,
# third-party code included, which would otherwise warn in every file that
# includes them. Abseil's own targets turn the warning back on, so it is added
# to them again after their own options (see below). Game Bits' libraries pass
# it on as they do -pthread.
if (EMSCRIPTEN)
  add_compile_options(-Wno-deprecated-pragma)
  option(GB_WEB_THREADS "Build for the web with threads" OFF)
  if (GB_WEB_THREADS)
    add_compile_options(-pthread)
    add_link_options(-pthread)
  endif()
endif()

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

# Adds -Wno-deprecated-pragma to every target that compiles in DIR and the
# directories below it, after the targets' own options. It is a SHELL: group,
# as CMake drops an option a target already has (here from
# add_compile_options), keeping only the first.
function(gb_web_no_deprecated_pragma DIR)
  get_property(targets DIRECTORY "${DIR}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target ${targets})
    get_target_property(type ${target} TYPE)
    if (NOT type STREQUAL "INTERFACE_LIBRARY")
      target_compile_options(${target} PRIVATE "SHELL:-Wno-deprecated-pragma")
    endif()
  endforeach()
  get_property(subdirs DIRECTORY "${DIR}" PROPERTY SUBDIRECTORIES)
  foreach(subdir ${subdirs})
    gb_web_no_deprecated_pragma("${subdir}")
  endforeach()
endfunction()
if (EMSCRIPTEN)
  gb_web_no_deprecated_pragma("${GB_THIRD_PARTY_DIR}/abseil-cpp")
endif()

####### re2

add_subdirectory("${GB_THIRD_PARTY_DIR}/re2")
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(re2 PRIVATE ${GB_3P_MSVC_COMPILE_OPTIONS})
endif()

####### glm

option(GLM_BUILD_INSTALL "" OFF)
set(GLM_BUILD_INSTALL OFF)
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

# flatbuffers builds flatc from source, and flatbuffers_generate_headers uses
# that target, so generated code always matches the runtime headers.
option(FLATBUFFERS_BUILD_TESTS "" OFF)
set(FLATBUFFERS_BUILD_TESTS OFF)
option(FLATBUFFERS_INSTALL "" OFF)
set(FLATBUFFERS_INSTALL OFF)
add_subdirectory("${GB_THIRD_PARTY_DIR}/flatbuffers")
set_target_properties(flatbuffers flatc PROPERTIES FOLDER External/flatbuffers)

# The build runs flatc, and a Debug flatc linked to the debug CRT DLL can't
# start, as that DLL isn't on PATH. Linking it statically lets it run anywhere.
# MSVC_RUNTIME_LIBRARY needs policy CMP0091 NEW (CMake 3.15) at the build's
# first project(). Where it is OLD, flatc gets /MTd after CMake's own /MDd.
if (MSVC)
  if (CMAKE_MSVC_RUNTIME_LIBRARY_DEFAULT)
    set_target_properties(flatc PROPERTIES MSVC_RUNTIME_LIBRARY
      "$<IF:$<CONFIG:Debug>,MultiThreadedDebug,MultiThreadedDLL>")
  else()
    target_compile_options(flatc PRIVATE $<$<CONFIG:Debug>:/MTd>)
  endif()
endif()
unset(FLATBUFFERS_BUILD_TESTS)
unset(FLATBUFFERS_INSTALL)

# In a web build, the build runs flatc under Node, where it reads and writes
# the machine's files.
gb_set_web_program_properties(flatc)

####### libccd
# Instead of add_subdirectory, define the ccd target directly to avoid
# libccd's outdated cmake_minimum_required(VERSION 2.8.11).

set(LIBCCD_SRC_DIR "${GB_THIRD_PARTY_DIR}/libccd/src")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/libccd/ccd/config.h"
  "#ifndef __CCD_CONFIG_H__\n"
  "#define __CCD_CONFIG_H__\n"
  "\n"
  "#define CCD_SINGLE\n"
  "/* #undef CCD_DOUBLE */\n"
  "\n"
  "#endif /* __CCD_CONFIG_H__ */\n"
)

add_library(ccd
  "${LIBCCD_SRC_DIR}/ccd/ccd.h"
  "${LIBCCD_SRC_DIR}/ccd/compiler.h"
  "${LIBCCD_SRC_DIR}/ccd/ccd_export.h"
  "${LIBCCD_SRC_DIR}/ccd/quat.h"
  "${LIBCCD_SRC_DIR}/ccd/vec3.h"
  "${CMAKE_CURRENT_BINARY_DIR}/libccd/ccd/config.h"
  "${LIBCCD_SRC_DIR}/alloc.h"
  "${LIBCCD_SRC_DIR}/ccd.c"
  "${LIBCCD_SRC_DIR}/dbg.h"
  "${LIBCCD_SRC_DIR}/list.h"
  "${LIBCCD_SRC_DIR}/mpr.c"
  "${LIBCCD_SRC_DIR}/polytope.c"
  "${LIBCCD_SRC_DIR}/polytope.h"
  "${LIBCCD_SRC_DIR}/simplex.h"
  "${LIBCCD_SRC_DIR}/support.c"
  "${LIBCCD_SRC_DIR}/support.h"
  "${LIBCCD_SRC_DIR}/vec3.c"
)
target_include_directories(ccd PUBLIC
  "${LIBCCD_SRC_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/libccd"
)
set_target_properties(ccd PROPERTIES FOLDER External/ccd)
target_compile_definitions(ccd PUBLIC CCD_STATIC_DEFINE)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(ccd PRIVATE ${GB_3P_MSVC_COMPILE_OPTIONS})
endif()

####### SDL
# A static library, for windowing and input, so its audio and its own renderer
# are left out. Programs link sdl2, and sdl2_main for SDL's main(). On Linux,
# SDL can only open windows if the system's windowing headers (X11, Wayland)
# are installed when it is built (see CLAUDE.md).

set(SDL_TEST OFF)
set(SDL_AUDIO OFF)
set(SDL_RENDER OFF)
set(SDL2_DISABLE_UNINSTALL ON)
add_subdirectory("${GB_THIRD_PARTY_DIR}/SDL")
set_target_properties(SDL2-static SDL2main PROPERTIES FOLDER External/SDL)
add_library(sdl2 ALIAS SDL2-static)
add_library(sdl2_main ALIAS SDL2main)
unset(SDL_TEST)
unset(SDL_AUDIO)
unset(SDL_RENDER)
unset(SDL2_DISABLE_UNINSTALL)

###############################################################################
## Add Game Bits source
###############################################################################

add_subdirectory(src)
