// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/test/log_recorder.h"

#include <thread>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;

TEST(LogRecorderTest, RecordsMessage) {
  LogRecorder log_recorder;
  const int line = __LINE__ + 1;
  LOG(WARNING) << "Test warning";
  std::vector<LogRecorder::Message> messages = log_recorder.TakeMessages();
  ASSERT_EQ(messages.size(), 1);
  EXPECT_EQ(messages[0].severity, absl::LogSeverity::kWarning);
  EXPECT_EQ(messages[0].file, __FILE__);
  EXPECT_EQ(messages[0].line, line);
  EXPECT_EQ(messages[0].text, "Test warning");
  EXPECT_EQ(absl::StrCat(messages[0]),
            absl::StrCat(__FILE__, ":", line, ": WARNING: Test warning"));
}

TEST(LogRecorderTest, RecordsAllSeveritiesInOrder) {
  LogRecorder log_recorder;
  LOG(INFO) << "Test info";
  LOG(ERROR) << "Test error";
  LOG(WARNING) << "Test warning";
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text, "Test info"),
                          Field(&LogRecorder::Message::text, "Test error"),
                          Field(&LogRecorder::Message::text, "Test warning")));
}

TEST(LogRecorderTest, IgnoresLowerSeverity) {
  LogRecorder log_recorder(absl::LogSeverityAtLeast::kWarning);
  LOG(INFO) << "Test info";
  LOG(WARNING) << "Test warning";
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text, "Test warning")));
}

TEST(LogRecorderTest, TakeClearsMessages) {
  LogRecorder log_recorder;
  LOG(INFO) << "First";
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text, "First")));
  EXPECT_THAT(log_recorder.TakeMessages(), IsEmpty());
  LOG(INFO) << "Second";
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text, "Second")));
}

TEST(LogRecorderTest, RecordersAreIndependent) {
  LogRecorder log_recorder_1;
  LogRecorder log_recorder_2;
  LOG(INFO) << "Test info";
  EXPECT_THAT(log_recorder_1.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text, "Test info")));
  EXPECT_THAT(log_recorder_2.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text, "Test info")));
}

TEST(LogRecorderTest, RecordsOtherThreads) {
  LogRecorder log_recorder;
  std::thread thread([] { LOG(INFO) << "Thread info"; });
  thread.join();
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text, "Thread info")));
}

TEST(LogRecorderTest, NothingRecordedAfterDestroyed) {
  {
    LogRecorder log_recorder;
  }
  LOG(INFO) << "Unrecorded info";
}

}  // namespace
}  // namespace gb
