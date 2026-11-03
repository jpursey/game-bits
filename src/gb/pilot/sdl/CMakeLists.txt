## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_pilot_sdl_SOURCE
  pilot_sdl.cc pilot_sdl.h
)

set(gb_pilot_sdl_TEST_SOURCE
  pilot_sdl_test.cc
)

set(gb_pilot_sdl_TEST_DEPS
  gb_test
)

set(gb_pilot_sdl_DEPS
  absl::memory
  gb_pilot
)

set(gb_pilot_sdl_LIBS
  absl::check
  absl::log
  sdl2
)

gb_add_library(gb_pilot_sdl)

# The test calls SDL, so it needs SDL's DLL beside it.
if (TARGET gb_pilot_sdl_test)
  add_custom_command(TARGET gb_pilot_sdl_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:sdl2> $<TARGET_FILE_DIR:gb_pilot_sdl_test>
  )
endif()
