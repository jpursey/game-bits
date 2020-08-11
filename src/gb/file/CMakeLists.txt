## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_file_SOURCE
  chunk_reader.cc chunk_reader.h
  chunk_types.h
  chunk_writer.cc chunk_writer.h
  file.cc file.h
  file_protocol.cc file_protocol.h
  file_system.cc file_system.h
  file_types.h
  local_file_protocol.cc local_file_protocol.h
  memory_file_protocol.cc memory_file_protocol.h
  path.cc path.h
  raw_file.h
)

set(gb_file_TEST_SOURCE
  chunk_file_test.cc
  common_protocol_test.cc common_protocol_test.h
  path_test.cc
  file_protocol_test.cc
  file_system_test.cc
  file_test.cc
  local_file_protocol_test.cc
  memory_file_protocol_test.cc
  test_protocol.cc test_protocol.h
  test_protocol_test.cc
)

set(gb_file_DEPS
  absl::flat_hash_map
  gb_base
  gb_test
)

set(gb_file_LIBS
  glog
)

gb_add_library(gb_file)
