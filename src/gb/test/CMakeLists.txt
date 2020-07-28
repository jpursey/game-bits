## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_test_SOURCE
  test_util.cc test_util.h
  thread_tester.cc thread_tester.h
)

set(gb_test_TEST_SOURCE
  thread_tester_test.cc
)

set(gb_test_DEPS
  absl::flat_hash_map
  absl::strings
  absl::synchronization 
)

set(gb_test_LIBS
  glog
)

gb_add_library(gb_test)
