## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_cli_SOURCE
  cli.cc cli.h
  cli_command_line.cc cli_command_line.h
  cli_help.cc cli_help.h
  cli_internal.cc cli_internal.h
  cli_path.cc cli_path.h
  cli_validators.cc cli_validators.h
  cli_writer.cc cli_writer.h
  run_command_line.cc run_command_line.h
)

set(gb_cli_DEPS
  absl::flat_hash_map
  absl::flat_hash_set
  absl::status
  absl::statusor
  absl::strings
  gb_base
  gb_file
)

set(gb_cli_LIBS
  absl::check
)

set(gb_cli_TEST_SOURCE
  cli_command_line_test.cc
  cli_help_test.cc
  cli_path_test.cc
  cli_test.cc
  cli_validators_test.cc
  fake_cli_writer.h
  run_command_line_test.cc
)

gb_add_library(gb_cli)
