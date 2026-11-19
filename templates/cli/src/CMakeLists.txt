set(cli_template_SOURCE
  main.cc
)

set(cli_template_DEPS
  absl::log
  absl::log_globals
  gb_base
)

gb_add_executable(cli_template)
