## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_base_SOURCE
  access_token.h
  callback.h
  callback_scope.h
  circular_queue.h
  clock.h
  context.cc context.h
  context_builder.h
  fake_clock.h
  flags.h
  platform_requirements.cc platform_requirements.h
  scoped_call.h
  string_util.h
  type_info.h
  validated_context.cc validated_context.h
  weak_ptr.h
)

set(gb_base_TEST_SOURCE
  callback_test.cc
  callback_scope_test.cc
  circular_queue_test.cc
  clock_test.cc
  context_test.cc
  flags_test.cc
  type_info_test.cc
  validated_context_test.cc
  weak_ptr_test.cc
)
set(gb_base_TEST_DEPS
  gb_test
)
set_source_files_properties(validated_context_test.cc
  PROPERTIES COMPILE_FLAGS /bigobj
)

set(gb_base_DEPS
  absl::flat_hash_map
  absl::memory
  absl::strings
  absl::synchronization
  absl::time
)

set(gb_base_LIBS
  glog
)

gb_add_library(gb_base)
