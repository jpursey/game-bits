## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(stb_image_SOURCE
  ${GB_THIRD_PARTY_DIR}/stb/stb_image.h
  stb_image.cc
)

set(stb_image_INCLUDES
  ${GB_THIRD_PARTY_DIR}/stb
)

gb_add_library(stb_image)

set(stb_image_resize_SOURCE
  ${GB_THIRD_PARTY_DIR}/stb/stb_image_resize.h
  stb_image_resize.cc
)

set(stb_image_resize_INCLUDES
  ${GB_THIRD_PARTY_DIR}/stb
)

gb_add_library(stb_image_resize)

set(stb_perlin_SOURCE
  ${GB_THIRD_PARTY_DIR}/stb/stb_perlin.h
  stb_perlin.cc
)

set(stb_perlin_INCLUDES
  ${GB_THIRD_PARTY_DIR}/stb
)

gb_add_library(stb_perlin)
