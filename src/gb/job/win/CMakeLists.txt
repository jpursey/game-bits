## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_job_internal_SOURCE
  win_job_fiber.cc
)

set(gb_job_internal_DEPS
  gb_base
)

set(gb_job_internal_LIBS
  glog
)

gb_add_library(gb_job_internal)
