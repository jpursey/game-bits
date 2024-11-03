## Copyright (c) 2024 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_parse_SOURCE
  lexer.cc lexer.h
  lexer_config.h
  parse_error.cc parse_error.h
  parse_result.cc parse_result.h
  parse_types.h
  parser.cc parser.h
  parser_program.cc parser_program.h
  parser_rules.cc parser_rules.h
  symbol.h
  token.cc token.h
)

set(gb_parse_DEPS
  absl::strings
  gb_base
  re2
)

set(gb_parse_LIBS
  absl::check
  absl::log
)

set(gb_parse_TEST_SOURCE
  lexer_test.cc
  parser_program_test.cc
  parser_rules_test.cc
  parser_test.cc
)

gb_add_library(gb_parse)
