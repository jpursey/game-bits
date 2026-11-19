set(sdl_template_SOURCE
  main.cc
)

set(sdl_template_DEPS
  gb_base
  sdl2
  sdl2_main
)

set(sdl_template_LIBS
  absl::log
  absl::log_globals
)

gb_add_win_executable(sdl_template)
