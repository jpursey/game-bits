## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

add_subdirectory(sdl)

set(gb_imgui_SOURCE
  gb_imconfig.h
  imgui_instance.cc imgui_instance.h
  imgui_types.h
  ${GB_THIRD_PARTY_DIR}/imgui/imgui.cpp
  ${GB_THIRD_PARTY_DIR}/imgui/imgui.h
  ${GB_THIRD_PARTY_DIR}/imgui/imgui_draw.cpp
  ${GB_THIRD_PARTY_DIR}/imgui/imgui_internal.h
  ${GB_THIRD_PARTY_DIR}/imgui/imgui_tables.cpp
  ${GB_THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp
  ${GB_THIRD_PARTY_DIR}/imgui/imstb_rectpack.h
  ${GB_THIRD_PARTY_DIR}/imgui/imstb_textedit.h
  ${GB_THIRD_PARTY_DIR}/imgui/imstb_truetype.h
)

# Draws with gb_render's test backend.
set(gb_imgui_RENDER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../render)
set(gb_imgui_TEST_SOURCE
  imgui_instance_test.cc
  ${gb_imgui_RENDER_TEST_DIR}/render_test.cc
  ${gb_imgui_RENDER_TEST_DIR}/test_binding_data.cc
  ${gb_imgui_RENDER_TEST_DIR}/test_render_backend.cc
  ${gb_imgui_RENDER_TEST_DIR}/test_render_buffer.cc
  ${gb_imgui_RENDER_TEST_DIR}/test_render_pipeline.cc
  ${gb_imgui_RENDER_TEST_DIR}/test_shader_code.cc
  ${gb_imgui_RENDER_TEST_DIR}/test_texture.cc
  ${gb_imgui_RENDER_TEST_DIR}/test_texture_array.cc
)

set(gb_imgui_INCLUDES
  ${GB_THIRD_PARTY_DIR}/imgui
)

set(gb_imgui_DEFINES
  IMGUI_USER_CONFIG="gb/imgui/gb_imconfig.h"
  IMGUI_DISABLE_OBSOLETE_FUNCTIONS
)

set(gb_imgui_DEPS
  absl::flat_hash_map
  absl::memory
  gb_base
  gb_file
  gb_render
  gb_resource
)

gb_add_library(gb_imgui)
