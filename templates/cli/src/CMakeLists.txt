set(cli_template_SOURCE
  main.cc
)

set(cli_template_DEPS
  gb_base
)

set(cli_template_LIBS
  absl::log
  absl::log_globals
)

gb_add_executable(cli_template)
