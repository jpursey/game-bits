## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_game_SOURCE
  game.cc game.h
  game_state.cc game_state.h
  game_state_machine.cc game_state_machine.h
)

set(gb_game_TEST_SOURCE
  game_state_machine_test.cc
  game_test.cc
)

set(gb_game_TEST_DEPS
  gb_test
)

set(gb_game_DEPS
  absl::flat_hash_map
  absl::strings
  absl::time
  gb_base
)

set(gb_base_LIBS
  absl::check
  absl::log
)

gb_add_library(gb_game)
