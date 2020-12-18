## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_alloc_SOURCE
  pool_allocator.cc pool_allocator.h
)

set(gb_alloc_TEST_SOURCE
  pool_allocator_test.cc
  test_allocator.cc
)
set(gb_alloc_TEST_DEPS
  absl::flat_hash_map
  absl::flat_hash_set
)

set(gb_alloc_DEPS
  gb_base
)
set(gb_alloc_LIBS
  glog
)

gb_add_library(gb_alloc)
