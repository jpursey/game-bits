## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_resource_SOURCE
  resource.cc resource.h
  resource_entry.cc resource_entry.h
  resource_manager.cc resource_manager.h
  resource_ptr.h
  resource_set.cc resource_set.h
  resource_system.cc resource_system.h
  resource_types.h
)

set(gb_resource_DEPS
  absl::flat_hash_map
  absl::flat_hash_set
  absl::memory
  absl::synchronization
  gb_base
)

set(gb_resource_TEST_SOURCE
  resource_test.cc
  resource_loader_test.cc
  resource_set_test.cc
  resource_thread_test.cc
  test_resources.h
)

set(gb_resource_TEST_DEPS
  gb_test
)

set(gb_resource_LIBS
  glog
)

gb_add_library(gb_resource)
