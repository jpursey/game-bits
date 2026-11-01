## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_cli_SOURCE
  cli.cc cli.h
  cli_internal.cc cli_internal.h
  cli_validators.cc cli_validators.h
  cli_values.cc cli_values.h
)

set(gb_cli_DEPS
  absl::flat_hash_map
  absl::flat_hash_set
  absl::status
  absl::statusor
  absl::strings
)

set(gb_cli_LIBS
  absl::check
)

set(gb_cli_TEST_SOURCE
  cli_test.cc
  cli_validators_test.cc
  cli_values_test.cc
)

gb_add_library(gb_cli)
