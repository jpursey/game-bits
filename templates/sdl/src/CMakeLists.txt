set(sdl_template_SOURCE
  main.cc
)

set(sdl_template_LIBS
  absl::log
  absl::log_globals
  absl::log_initialize
  sdl2
  sdl2_main
)

gb_add_win_executable(sdl_template)
