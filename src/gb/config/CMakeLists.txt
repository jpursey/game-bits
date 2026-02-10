## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_config_SOURCE
  config.cc config.h
)

set(gb_config_DEPS
  absl::strings
  gb_base
)

set(gb_config_TEST_SOURCE
  config_test.cc
)

gb_add_library(gb_config)
