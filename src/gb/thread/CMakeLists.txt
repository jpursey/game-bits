## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

if (WIN32)
  set(gb_thread_internal_SOURCE
    win_fiber.cc
    win_thread.cc
  )
  set(gb_thread_internal_DEPS
    absl::strings
    absl::str_format
    absl::synchronization
    gb_base
  )
  set(gb_thread_internal_LIBS
    absl::check
    absl::log
  )
else()
  set(gb_thread_internal_SOURCE
    gen_fiber.cc
    gen_thread.cc
  )
  set(gb_thread_internal_DEPS
    absl::strings
    absl::str_format
    absl::synchronization
    gb_base
  )
  set(gb_thread_internal_LIBS
    absl::check
    absl::log
  )
endif()

set(gb_thread_SOURCE
  fiber.h
  thread.h
  thread_types.h
  ${gb_thread_internal_SOURCE}
)

set(gb_thread_DEPS
  gb_base
  ${gb_thread_internal_DEPS}
)

set(gb_thread_LIBS
  ${gb_thread_internal_LIBS}
)

set(gb_thread_TEST_SOURCE
  fiber_test.cc
  thread_test.cc
)

set(gb_thread_TEST_DEPS
  absl::flat_hash_map
  absl::synchronization
  absl::time
  gb_base
)

set(gb_thread_TEST_LIBS
  absl::check
  absl::log
)

gb_add_library(gb_thread)
