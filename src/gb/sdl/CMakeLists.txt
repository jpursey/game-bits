## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_sdl_SOURCE
  sdl_glog.cc sdl_glog.h
)

set(gb_sdl_LIBS
  glog
  sdl2
)

gb_add_library(gb_sdl)
