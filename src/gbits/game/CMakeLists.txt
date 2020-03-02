set(gbits_game_SOURCE
  game.cc game.h
)

set(gbits_game_TEST_SOURCE
  game_test.cc
)

set(gbits_game_DEPS
  absl::time
  gbits_base
)

gbits_add_library(gbits_game)
