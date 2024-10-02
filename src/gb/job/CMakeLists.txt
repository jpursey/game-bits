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

set(gb_job_DEPS
  absl::strings
  absl::str_format
  absl::synchronization
  gb_alloc
  gb_base
  gb_thread
)

set(gb_job_LIBS
  absl::check
  absl::log
)

gb_add_library(gb_job)
