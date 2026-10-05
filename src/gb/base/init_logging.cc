// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/init_logging.h"

#include "absl/base/call_once.h"
#include "absl/log/initialize.h"

namespace gb {
namespace {

absl::once_flag g_init_logging_once;

}  // namespace

void InitLogging() {
  absl::call_once(g_init_logging_once, absl::InitializeLog);
}

}  // namespace gb
