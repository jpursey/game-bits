set(sdl_template_SOURCE
  main.cc
)

set(sdl_template_DEPS
  absl::log
  absl::log_globals
  absl::statusor
  absl::time
  gb_base
  gb_cli
  gb_game
  gb_pilot
  gb_pilot_script
  gb_pilot_sdl
  sdl2
  sdl2_main
)

gb_add_win_executable(sdl_template)
