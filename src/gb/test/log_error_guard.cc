// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/test/log_error_guard.h"

#include "gtest/gtest.h"

namespace gb {

LogErrorGuard::LogErrorGuard(absl::LogSeverityAtLeast min_severity)
    : log_recorder_(min_severity) {}

LogErrorGuard::~LogErrorGuard() {
  for (const LogRecorder::Message& message : log_recorder_.TakeMessages()) {
    ADD_FAILURE_AT(message.file.c_str(), message.line)
        << "Logged " << absl::LogSeverityName(message.severity) << ": "
        << message.text;
  }
}

}  // namespace gb
