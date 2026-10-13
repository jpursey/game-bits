## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_job_SOURCE
  fiber_job_system.cc fiber_job_system.h
  job_counter.h
  job_system.cc job_system.h
  job_types.h
)

set(gb_job_INCLUDES
  ${GB_THIRD_PARTY_DIR}/concurrentqueue
)

set(gb_job_TEST_SOURCE
  fiber_job_system_test.cc
)

set(gb_job_TEST_DEPS
  absl::str_format
  gb_test
)

set(gb_job_DEPS
  absl::any_invocable
  absl::check
  absl::flat_hash_set
  absl::inlined_vector
  absl::log
  absl::memory
  absl::strings
  absl::synchronization
  absl::time
  gb_alloc
  gb_base
  gb_thread
)

gb_add_library(gb_job)
