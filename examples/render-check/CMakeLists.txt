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
  render_check.cc render_check.h
)

set(render_check_DEPS
  absl::log_entry
  absl::log_sink
  absl::log_sink_registry
  absl::statusor
  absl::str_format
  absl::strings
  absl::synchronization
  absl::time
  gb_base
  gb_cli
  gb_file
  gb_game
  gb_imgui
  gb_imgui_sdl
  gb_message
  gb_pilot
  gb_pilot_script
  gb_pilot_sdl
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

gb_embed_shader(render_check shaders/pool.vert shaders/pool_vert.h
  kPoolVertexShader)
gb_embed_shader(render_check shaders/quad.frag shaders/quad_frag.h
  kQuadFragmentShader)
gb_embed_shader(render_check shaders/quad.vert shaders/quad_vert.h
  kQuadVertexShader)
gb_embed_shader(render_check shaders/storage.frag shaders/storage_frag.h
  kStorageFragmentShader)
gb_embed_shader(render_check shaders/storage.vert shaders/storage_vert.h
  kStorageVertexShader)
gb_embed_shader(render_check shaders/unset.frag shaders/unset_frag.h
  kUnsetFragmentShader)
gb_embed_shader(render_check shaders/volume.frag shaders/volume_frag.h
  kVolumeFragmentShader)
gb_embed_shader(render_check shaders/volume.vert shaders/volume_vert.h
  kVolumeVertexShader)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT render_check)
