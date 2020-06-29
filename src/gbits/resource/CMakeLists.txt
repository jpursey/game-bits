set(gbits_resource_SOURCE
  resource.cc resource.h
  resource_entry.cc resource_entry.h
  resource_manager.cc resource_manager.h
  resource_ptr.h
  resource_set.cc resource_set.h
  resource_system.cc resource_system.h
  resource_types.h
)

set(gbits_resource_DEPS
  absl::flat_hash_map
  absl::flat_hash_set
  absl::memory
  absl::synchronization
  gbits_base
)

set(gbits_resource_TEST_SOURCE
  resource_test.cc
  resource_loader_test.cc
  resource_set_test.cc
  resource_thread_test.cc
  test_resources.h
)

set(gbits_resource_TEST_DEPS
  gbits_test
)

set(gbits_resource_LIBS
  glog
)

gbits_add_library(gbits_resource)
