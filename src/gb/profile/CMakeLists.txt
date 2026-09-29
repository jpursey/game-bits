## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_profile_SOURCE
  fake_ticks.h
  profile_call_hook.h
  profile_point.cc profile_point.h
  profile_timer.h
  profiler.cc profiler.h
)

set(gb_profile_TEST_SOURCE
  profile_call_hook_test.cc
  profile_point_test.cc
  profile_timer_test.cc
  profiler_test.cc
)

set(gb_profile_DEPS
  absl::algorithm_container
  absl::flat_hash_map
  absl::no_destructor
  absl::span
  absl::str_format
  absl::strings
  absl::synchronization
  absl::time
  gb_base
)

set(gb_profile_LIBS
  absl::check
  absl::log
)

gb_add_library(gb_profile)
