set(game_SOURCE
	game.cc game.h
)

set(game_DEPS
	base
)

set(game_INCLUDES
	${GBITS_ABSL_INCLUDE}
)

set(game_LIBS
	${GBITS_ABSL_LIB_TIME}
)

gbits_add_library(game)
