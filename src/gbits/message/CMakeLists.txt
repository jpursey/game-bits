set(gbits_message_SOURCE
  message_dispatcher.cc message_dispatcher.h
  message_endpoint.cc message_endpoint.h
  message_types.h
  message_system.cc message_system.h
)

set(gbits_message_TEST_SOURCE
  message_system_test.cc
)

set(gbits_message_TEST_DEPS
  gbits_test
)

set(gbits_message_DEPS
  absl::flat_hash_map
  absl::flat_hash_set
  gbits_base
)

set(gbits_base_LIBS
  glog
)

gbits_add_library(gbits_message)
