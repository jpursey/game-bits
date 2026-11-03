## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

add_subdirectory(sdl)

set(gb_pilot_SOURCE
  pilot.cc pilot.h
  fake_pilot.cc fake_pilot.h
  step_pilot.cc step_pilot.h
)

set(gb_pilot_TEST_SOURCE
  pilot_test.cc
  fake_pilot_test.cc
  step_pilot_test.cc
)

set(gb_pilot_TEST_DEPS
  gb_test
)

set(gb_pilot_DEPS
  absl::any_invocable
  absl::check
  absl::log
  absl::log_entry
  absl::log_severity
  absl::log_sink
  absl::log_sink_registry
  absl::span
  absl::status
  absl::statusor
  absl::str_format
  absl::strings
  absl::time
  gb_image
)

gb_add_library(gb_pilot)
