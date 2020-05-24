set(gbits_test_SOURCE
  test_util.cc test_util.h
  thread_tester.cc thread_tester.h
)

set(gbits_test_TEST_SOURCE
  thread_tester_test.cc
)

set(gbits_test_DEPS
  absl::flat_hash_map
  absl::strings
  absl::synchronization 
)

set(gbits_test_LIBS
  glog
)

gbits_add_library(gbits_test)
