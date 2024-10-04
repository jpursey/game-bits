## Copyright (c) 2024 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_parse_SOURCE
  lexer.cc lexer.h
  lexer_config.h
  lexer_types.h
  symbol.h
  token.h
)

set(gb_parse_DEPS
  absl::strings
  gb_base
)

set(gb_parse_LIBS
  absl::check
  absl::log
)

set(gb_parse_TEST_SOURCE
  lexer_test.cc
)

gb_add_library(gb_parse)
