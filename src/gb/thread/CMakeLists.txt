## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

if (WIN32)
  set(gb_thread_internal_SOURCE
    win_cpu_topology.cc win_cpu_topology.h
    win_fiber.cc
    win_thread.cc
  )
  set(gb_thread_internal_DEPS
    absl::bits
    absl::no_destructor
    absl::strings
    absl::str_format
    absl::synchronization
    gb_base
  )
  set(gb_thread_internal_LIBS
    absl::check
    absl::log
  )
  set(gb_thread_internal_TEST_SOURCE
    win_cpu_topology_test.cc
    win_thread_test.cc
  )
else()
  set(gb_thread_internal_SOURCE
    gen_cpu_topology.cc
    gen_fiber.cc
    gen_thread.cc
  )
  set(gb_thread_internal_DEPS
    absl::no_destructor
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
  cpu_ticks.cc cpu_ticks.h
  cpu_ticks_internal.cc cpu_ticks_internal.h
  cpu_topology.h
  cpu_topology_internal.cc cpu_topology_internal.h
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
  cpu_ticks_test.cc
  cpu_topology_test.cc
  fiber_test.cc
  thread_test.cc
  ${gb_thread_internal_TEST_SOURCE}
)

set(gb_thread_TEST_DEPS
  absl::algorithm_container
  absl::cleanup
  absl::flat_hash_map
  absl::flat_hash_set
  absl::synchronization
  gb_base
)

set(gb_thread_TEST_LIBS
  absl::check
  absl::log
)

gb_add_library(gb_thread)
