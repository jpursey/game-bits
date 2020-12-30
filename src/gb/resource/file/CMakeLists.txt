## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_resource_file_SOURCE
  resource_file_reader.cc resource_file_reader.h
  resource_file_writer.cc resource_file_writer.h
  resource_chunks.h
)

set(gb_resource_file_DEPS
  absl::flat_hash_map
  absl::memory
  gb_file
  gb_resource
)

set(gb_resource_file_LIBS
  flatbuffers
  glog
)

set(gb_resource_file_TEST_SOURCE
  resource_file_test.cc
)

set(gb_resource_file_TEST_FBS
  resource_file_test.fbs
)

gb_add_library(gb_resource_file)
