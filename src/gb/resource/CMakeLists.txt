## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_resource_SOURCE
  resource.cc resource.h
  resource_chunks.h
  resource_entry.cc resource_entry.h
  resource_file_reader.cc resource_file_reader.h
  resource_file_writer.cc resource_file_writer.h
  resource_manager.cc resource_manager.h
  resource_name_reservation.cc resource_name_reservation.h
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
  gb_file
)

set(gb_resource_LIBS
  flatbuffers
  absl::check
  absl::log
)

set(gb_resource_TEST_SOURCE
  resource_file_test.cc
  resource_loader_test.cc
  resource_set_test.cc
  resource_test.cc
  resource_thread_test.cc
  test_resources.h
)

set(gb_resource_TEST_FBS
  resource_file_test.fbs
)

set(gb_resource_TEST_DEPS
  gb_test
)

gb_add_library(gb_resource)

