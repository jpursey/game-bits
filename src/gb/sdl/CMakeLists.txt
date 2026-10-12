## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_sdl_SOURCE
  sdl_glog.cc sdl_glog.h
)

set(gb_sdl_LIBS
  absl::check
  absl::log
  sdl2
)

set(gb_sdl_TEST_SOURCE
  sdl_glog_test.cc
)
set(gb_sdl_TEST_DEPS
  gb_test
)

gb_add_library(gb_sdl)
