## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

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
  absl::algorithm_container
  absl::any_invocable
  absl::check
  absl::flat_hash_map
  absl::flat_hash_set
  absl::inlined_vector
  absl::log
  absl::memory
  absl::synchronization
  gb_base
)

gb_add_library(gb_message)
