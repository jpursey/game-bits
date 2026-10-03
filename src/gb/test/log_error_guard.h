// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_TEST_LOG_ERROR_GUARD_H_
#define GB_TEST_LOG_ERROR_GUARD_H_

#include <vector>

#include "absl/base/log_severity.h"
#include "gb/test/log_recorder.h"

namespace gb {

// Fails the current test for every message logged at or above a severity.
//
// For as long as it exists, a LogErrorGuard records every Abseil log message
// at or above its severity (ERROR by default), logged on any thread. When it is
// destroyed, it fails the current test once for each message still recorded,
// reporting the failure at the file and line that logged it. A test fixture
// holds one as a member, so that every test fails on an error logged anywhere
// in it, including its setup and teardown:
//
//   class FooTest : public ::testing::Test {
//    protected:
//     LogErrorGuard log_error_guard_;
//   };
//
// A test that means to log an error takes it with TakeMessages() and checks it
// itself. Taking messages from a separate LogRecorder doesn't take them from
// the guard, so the guard still fails on them.
class LogErrorGuard final {
 public:
  explicit LogErrorGuard(
      absl::LogSeverityAtLeast min_severity = absl::LogSeverityAtLeast::kError);
  LogErrorGuard(const LogErrorGuard&) = delete;
  LogErrorGuard& operator=(const LogErrorGuard&) = delete;
  ~LogErrorGuard();

  // Returns the messages recorded so far, in the order they were logged, and
  // clears them so they don't fail the test.
  std::vector<LogRecorder::Message> TakeMessages() {
    return log_recorder_.TakeMessages();
  }

 private:
  LogRecorder log_recorder_;
};

}  // namespace gb

#endif  // GB_TEST_LOG_ERROR_GUARD_H_
