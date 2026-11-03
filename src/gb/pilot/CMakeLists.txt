## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_pilot_SOURCE
  pilot.cc pilot.h
  fake_pilot.cc fake_pilot.h
)

set(gb_pilot_TEST_SOURCE
  pilot_test.cc
  fake_pilot_test.cc
)

set(gb_pilot_TEST_DEPS
  gb_test
)

set(gb_pilot_DEPS
  absl::any_invocable
  absl::check
  absl::log
  absl::span
  absl::status
  absl::statusor
  absl::strings
  absl::time
  gb_image
)

gb_add_library(gb_pilot)
