## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

if (WIN32)
  add_subdirectory(win)
else()
  add_subdirectory(gen)
endif()

set(gb_job_SOURCE
  fiber_job_system.cc fiber_job_system.h
  job_counter.h
  job_fiber.h
  job_system.cc job_system.h
  job_types.h
)

set(gb_job_TEST_SOURCE
  fiber_job_system_test.cc
  job_fiber_test.cc
)

set(gb_job_DEPS
  absl::strings
  absl::str_format
  absl::synchronization
  gb_alloc
  gb_base
  gb_job_internal
)

set(gb_job_LIBS
  glog
)

gb_add_library(gb_job)
