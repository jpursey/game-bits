## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_test_SOURCE
  log_error_guard.cc log_error_guard.h
  log_recorder.cc log_recorder.h
  test_util.cc test_util.h
  thread_tester.cc thread_tester.h
)

set(gb_test_TEST_SOURCE
  log_error_guard_test.cc
  log_recorder_test.cc
  thread_tester_test.cc
)

set(gb_test_DEPS
  absl::flat_hash_map
  absl::log_entry
  absl::log_severity
  absl::log_sink
  absl::log_sink_registry
  absl::str_format
  absl::strings
  absl::synchronization
  gtest
)

set(gb_test_LIBS
  absl::check
  absl::log
)

gb_add_library(gb_test)
