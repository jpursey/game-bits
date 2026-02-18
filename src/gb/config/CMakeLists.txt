## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_config_SOURCE
  config.cc config.h
  text_config.cc text_config.h
)

set(gb_config_DEPS
  absl::status
  absl::statusor
  absl::strings
  gb_base
  gb_parse
)

set(gb_config_TEST_SOURCE
  config_test.cc
  text_config_test.cc
)

gb_add_library(gb_config)
