set(sdl_template_SOURCE
  main.cc
)

set(sdl_template_LIBS
  glog
  sdl2
  sdl2_main
)

gb_add_win_executable(sdl_template)
