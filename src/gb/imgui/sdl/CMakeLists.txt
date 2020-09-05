## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_imgui_sdl_SOURCE
  ${GB_THIRD_PARTY_DIR}/imgui/examples/imgui_impl_sdl.cpp
  ${GB_THIRD_PARTY_DIR}/imgui/examples/imgui_impl_sdl.h
)

set(gb_imgui_sdl_INCLUDES
  ${GB_THIRD_PARTY_DIR}/imgui/examples
)

set(gb_imgui_sdl_DEPS
  gb_imgui
)

set(gb_imgui_sdl_LIBS
  sdl2
)

gb_add_library(gb_imgui_sdl)
