## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_pilot_script_SOURCE
  cli_pilot.cc cli_pilot.h
  pilot_script.cc pilot_script.h
)

set(gb_pilot_script_TEST_SOURCE
  cli_pilot_test.cc
  pilot_script_test.cc
)

set(gb_pilot_script_TEST_DEPS
  gb_test
)

set(gb_pilot_script_DEPS
  absl::check
  absl::log
  absl::no_destructor
  absl::status
  absl::statusor
  absl::strings
  gb_base
  gb_cli
  gb_file
  gb_image
  gb_parse
  gb_pilot
)

gb_add_library(gb_pilot_script)
