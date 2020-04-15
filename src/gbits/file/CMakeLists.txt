set(gbits_file_SOURCE
  file.cc file.h
  file_protocol.cc file_protocol.h
  file_system.cc file_system.h
  file_types.h
  path.cc path.h
  raw_file.h
)

set(gbits_file_TEST_SOURCE
  path_test.cc
  file_protocol_test.cc
  file_system_test.cc
  file_test.cc
  test_protocol.cc test_protocol.h
  test_protocol_test.cc
)

set(gbits_file_DEPS
  absl::flat_hash_map
  gbits_base
  gbits_test
)

set(gbits_file_LIBS
  glog
)

gbits_add_library(gbits_file)
