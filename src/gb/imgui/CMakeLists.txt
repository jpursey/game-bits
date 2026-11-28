## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

# The shaders, in the code each platform's render backend takes.
if (EMSCRIPTEN)
  set(gb_imgui_internal_SOURCE web_imgui_shaders.cc)
else()
  set(gb_imgui_internal_SOURCE gen_imgui_shaders.cc)
endif()

add_subdirectory(sdl)

set(gb_imgui_SOURCE
  ${gb_imgui_internal_SOURCE}
  gb_imconfig.h
  imgui_instance.cc imgui_instance.h
  imgui_pilot_hooks.cc imgui_pilot_hooks.h
  imgui_shaders.h
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

set(gb_imgui_TEST_SOURCE
  imgui_instance_test.cc
)

# Draws with gb_render's test backend.
set(gb_imgui_TEST_DEPS
  gb_render_testing
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
  absl::strings
  absl::time
  gb_base
  gb_file
  gb_pilot
  gb_render
  gb_resource
)

gb_add_library(gb_imgui)

# ImGui calls memset and memcpy on its own types that aren't trivially
# copyable, which newer versions of clang warn about, in its headers as well as
# its sources. Every target that includes them needs the warning off.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-Wnontrivial-memcall GB_HAS_NONTRIVIAL_MEMCALL)
  if (GB_HAS_NONTRIVIAL_MEMCALL)
    target_compile_options(gb_imgui PUBLIC -Wno-nontrivial-memcall)
  endif()
endif()
