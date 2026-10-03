// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/test/log_error_guard.h"

#include <thread>

#include "absl/log/log.h"
#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest-spi.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::ScopedFakeTestPartResultReporter;
using ::testing::TestPartResult;
using ::testing::TestPartResultArray;

TEST(LogErrorGuardTest, ErrorFailsTest) {
  TestPartResultArray results;
  int line = 0;
  {
    ScopedFakeTestPartResultReporter reporter(
        ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD,
        &results);
    LogErrorGuard log_error_guard;
    line = __LINE__ + 1;
    LOG(ERROR) << "Test error";
  }
  ASSERT_EQ(results.size(), 1);
  const TestPartResult& result = results.GetTestPartResult(0);
  EXPECT_TRUE(result.nonfatally_failed());
  EXPECT_STREQ(result.file_name(), __FILE__);
  EXPECT_EQ(result.line_number(), line);
  EXPECT_THAT(result.message(), HasSubstr("Logged ERROR: Test error"));
}

TEST(LogErrorGuardTest, EachErrorFailsTest) {
  TestPartResultArray results;
  {
    ScopedFakeTestPartResultReporter reporter(
        ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD,
        &results);
    LogErrorGuard log_error_guard;
    LOG(ERROR) << "First error";
    LOG(ERROR) << "Second error";
  }
  ASSERT_EQ(results.size(), 2);
  EXPECT_THAT(results.GetTestPartResult(0).message(), HasSubstr("First error"));
  EXPECT_THAT(results.GetTestPartResult(1).message(),
              HasSubstr("Second error"));
}

// The tests that expect no failure use a real LogErrorGuard, so a wrongly
// reported message fails them directly.
TEST(LogErrorGuardTest, WarningDoesNotFailTest) {
  LogErrorGuard log_error_guard;
  LOG(WARNING) << "Test warning";
}

TEST(LogErrorGuardTest, TakenMessagesDoNotFailTest) {
  LogErrorGuard log_error_guard;
  LOG(ERROR) << "Expected error";
  EXPECT_THAT(
      log_error_guard.TakeMessages(),
      ElementsAre(Field(&LogRecorder::Message::text, "Expected error")));
}

TEST(LogErrorGuardTest, ErrorAfterTakeFailsTest) {
  EXPECT_NONFATAL_FAILURE(
      {
        LogErrorGuard log_error_guard;
        LOG(ERROR) << "Expected error";
        log_error_guard.TakeMessages();
        LOG(ERROR) << "Unexpected error";
      },
      "Unexpected error");
}

TEST(LogErrorGuardTest, TakeFromLogRecorderStillFailsTest) {
  EXPECT_NONFATAL_FAILURE(
      {
        LogErrorGuard log_error_guard;
        LogRecorder log_recorder;
        LOG(ERROR) << "Test error";
        log_recorder.TakeMessages();
      },
      "Test error");
}

TEST(LogErrorGuardTest, LowerSeverityFailsTest) {
  TestPartResultArray results;
  {
    ScopedFakeTestPartResultReporter reporter(
        ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD,
        &results);
    LogErrorGuard log_error_guard(absl::LogSeverityAtLeast::kWarning);
    LOG(INFO) << "Test info";
    LOG(WARNING) << "Test warning";
    LOG(ERROR) << "Test error";
  }
  ASSERT_EQ(results.size(), 2);
  EXPECT_THAT(results.GetTestPartResult(0).message(),
              HasSubstr("Logged WARNING: Test warning"));
  EXPECT_THAT(results.GetTestPartResult(1).message(),
              HasSubstr("Logged ERROR: Test error"));
}

TEST(LogErrorGuardTest, ErrorOnOtherThreadFailsOnDestroyingThread) {
  // EXPECT_NONFATAL_FAILURE only intercepts failures on this thread.
  EXPECT_NONFATAL_FAILURE(
      {
        LogErrorGuard log_error_guard;
        std::thread thread([] { LOG(ERROR) << "Thread error"; });
        thread.join();
      },
      "Thread error");
}

}  // namespace
}  // namespace gb
