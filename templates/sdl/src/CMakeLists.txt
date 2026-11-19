set(sdl_template_SOURCE
  main.cc
)

set(sdl_template_DEPS
  absl::log
  absl::log_globals
  gb_base
  sdl2
  sdl2_main
)

gb_add_win_executable(sdl_template)
