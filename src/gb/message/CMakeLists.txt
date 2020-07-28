set(gb_message_SOURCE
  message_dispatcher.cc message_dispatcher.h
  message_endpoint.cc message_endpoint.h
  message_stack_endpoint.cc message_stack_endpoint.h
  message_system.cc message_system.h
  message_types.h
)

set(gb_message_TEST_SOURCE
  message_system_test.cc
  message_stack_endpoint_test.cc
)

set(gb_message_TEST_DEPS
  gb_test
)

set(gb_message_DEPS
  absl::flat_hash_map
  absl::flat_hash_set
  gb_base
)

set(gb_base_LIBS
  glog
)

gb_add_library(gb_message)
