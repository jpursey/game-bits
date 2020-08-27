## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_render_vulkan_sdl_SOURCE
  sdl_vulkan_window.cc sdl_vulkan_window.h
)

set(gb_render_vulkan_sdl_DEPS
  absl::synchronization
  gb_render_vulkan
)

set(gb_render_vulkan_sdl_LIBS
  glog
  sdl2
  Vulkan
)

gb_add_library(gb_render_vulkan_sdl)
