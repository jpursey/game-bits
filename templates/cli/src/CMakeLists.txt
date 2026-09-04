set(cli_template_SOURCE
  main.cc
)

set(cli_template_LIBS
  absl::log
  absl::log_globals
  absl::log_initialize
)

gb_add_executable(cli_template)
