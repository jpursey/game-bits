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
