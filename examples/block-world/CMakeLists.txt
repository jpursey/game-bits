## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

cmake_minimum_required (VERSION 3.8)

################################################################################
## Set project configuration
################################################################################

project(BlockWorld C CXX)

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
## Game Source
################################################################################

# Extra warnings and errors for example code
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Werror)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options(/WX)
endif()

set(block_world_SOURCE
  base_state.cc base_state.h
  block.h
  block_world.cc block_world.h
  camera.cc camera.h
  chunk.cc chunk.h
  cube.h
  game_types.h
  gui_fonts.h
  main.cc
  play_state.cc play_state.h
  scene_types.h
  states.cc states.h
  title_state.cc title_state.h
  world.cc world.h
  world_resources.cc world_resources.h
)

set(block_world_DEPS
  absl::strings
  gb_file
  gb_game
  gb_image
  gb_imgui
  gb_imgui_sdl
  gb_message
  gb_render
  gb_render_vulkan
  gb_render_vulkan_sdl
  gb_resource
  stb_perlin
)

set(block_world_LIBS
  absl::check
  absl::log
  sdl2
  sdl2_main
)

gb_add_win_executable(block_world)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT block_world)
