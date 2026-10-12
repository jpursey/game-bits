## Copyright (c) 2022 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_collide_SOURCE
  intersect.cc intersect.h
  shapes.cc shapes.h
)

set(gb_collide_DEPS
  absl::base
  absl::check
  absl::log
  absl::span
  ccd
)

set(gb_collide_LIBS
  glm
)

set(gb_collide_TEST_SOURCE
  intersect_test.cc
  shapes_test.cc
)

gb_add_library(gb_collide)
