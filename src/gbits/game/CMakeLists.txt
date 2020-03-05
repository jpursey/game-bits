set(gbits_game_SOURCE
  game.cc game.h
  game_state.cc game_state.h
  game_state_machine.cc game_state_machine.h
)

set(gbits_game_TEST_SOURCE
  game_test.cc
)

set(gbits_game_DEPS
  absl::flat_hash_map
  absl::strings
  absl::time
  gbits_base
)

set(gbits_base_LIBS
  glog
)

gbits_add_library(gbits_game)
