## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

cmake_minimum_required (VERSION 3.8)

################################################################################
## Set project configuration
################################################################################

project(RenderCheck C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

################################################################################
# Game Bits configuration
################################################################################

STRING(REGEX REPLACE "\\\\" "/" CURRENT_DIR ${CMAKE_CURRENT_LIST_DIR})
set(GB_DIR "${CURRENT_DIR}/../..")

add_subdirectory("${GB_DIR}" gb)
gb_config()
gb_external_libraries()

################################################################################
## Source
################################################################################

# Extra warnings and errors for example code
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Werror)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options(/WX)
endif()

set(render_check_SOURCE
  main.cc
  quad_shaders.h
  render_check.cc render_check.h
)

set(render_check_DEPS
  absl::log_entry
  absl::log_sink
  absl::log_sink_registry
  absl::str_format
  absl::strings
  absl::synchronization
  gb_file
  gb_imgui
  gb_imgui_sdl
  gb_message
  gb_render
  gb_render_vulkan
  gb_render_vulkan_sdl
  gb_resource
)

set(render_check_LIBS
  absl::log
  sdl2
  sdl2_main
)

gb_add_win_executable(render_check)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT render_check)
