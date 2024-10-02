## Copyright (c) 2021 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_image_SOURCE
  image.cc image.h
  image_file.cc image_file.h
  image_types.h
  image_view.cc image_view.h
  pixel.h
)

set(gb_image_DEPS
  absl::span
  gb_base
  gb_file
  stb_image
)

set(gb_image_LIBS
  absl::check
  absl::log
  glm
)

set(gb_image_TEST_SOURCE
  image_view_test.cc
)

gb_add_library(gb_image)
