// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <vector>

#include "absl/base/log_severity.h"
#include "gb/test/log_recorder.h"
#include "gb/thread/thread.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(WebThreadTest, DoesNotSupportThreads) { EXPECT_FALSE(SupportsThreads()); }

TEST(WebThreadTest, CreateThreadLogsErrorAndReturnsNull) {
  LogRecorder recorder(absl::LogSeverityAtLeast::kError);
  EXPECT_EQ(CreateThread(0, 0, nullptr, +[](void* user_data) {}), nullptr);
  const std::vector<LogRecorder::Message> messages = recorder.TakeMessages();
  ASSERT_EQ(messages.size(), 1);
  EXPECT_EQ(messages[0].severity, absl::LogSeverity::kError);
  EXPECT_EQ(GetActiveThreadCount(), 0);
}

TEST(WebThreadTest, MaxConcurrencyIsOne) { EXPECT_EQ(GetMaxConcurrency(), 1); }

}  // namespace
}  // namespace gb
