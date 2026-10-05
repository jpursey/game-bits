// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/log_file.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "gb/test/log_error_guard.h"
#include "gb/test/log_recorder.h"
#include "gb/test/thread_tester.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::MatchesRegex;
using ::testing::UnorderedElementsAreArray;

// Matches the start of a line, up to the source file name.
constexpr std::string_view kLinePrefixRegex =
    R"(I\d\d\d\d \d\d:\d\d:\d\d\.\d\d\d\d\d\d \d+ \(log_file_test\.cc:)";

class LogFileTest : public ::testing::Test {
 protected:
  LogFileTest()
      : path_(
            std::filesystem::path(::testing::TempDir()) /
            absl::StrCat(
                ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                ".log")) {}

  ~LogFileTest() override {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }

  // Returns the lines in the file at `path_`, or none if it doesn't exist.
  std::vector<std::string> ReadLines() {
#ifdef _WIN32
    std::FILE* file = _wfopen(path_.c_str(), L"r");
#else
    std::FILE* file = std::fopen(path_.c_str(), "r");
#endif
    if (file == nullptr) {
      return {};
    }
    std::string contents;
    char buffer[1024];
    int size = 0;
    while ((size = static_cast<int>(
                std::fread(buffer, 1, sizeof(buffer), file))) > 0) {
      contents.append(buffer, size);
    }
    std::fclose(file);
    return absl::StrSplit(contents, '\n', absl::SkipEmpty());
  }

  const std::filesystem::path path_;
  LogErrorGuard log_error_guard_;
};

TEST_F(LogFileTest, WritesFormattedLine) {
  LogFile log_file(path_, absl::LogSeverityAtLeast::kInfo);
  const int line = __LINE__ + 1;
  LOG(INFO) << "Hello";

  // Each message is flushed as it is written, so it can be read back while the
  // LogFile still exists.
  EXPECT_THAT(ReadLines(), ElementsAre(MatchesRegex(absl::StrCat(
                               kLinePrefixRegex, line, R"(\) Hello)"))));
}

TEST_F(LogFileTest, WritesOnlyWhileItExists) {
  LOG(INFO) << "Before";
  {
    LogFile log_file(path_, absl::LogSeverityAtLeast::kInfo);
    LOG(INFO) << "During";
  }
  LOG(INFO) << "After";

  EXPECT_THAT(ReadLines(), ElementsAre(EndsWith(") During")));
}

TEST_F(LogFileTest, WritesMessagesAtOrAboveMinLevel) {
  LogRecorder log_recorder;
  LogFile log_file(path_, absl::LogSeverityAtLeast::kWarning);
  LOG(INFO) << "Info";
  LOG(WARNING) << "Warning";
  LOG(ERROR) << "Error";
  log_error_guard_.TakeMessages();

  EXPECT_THAT(ReadLines(),
              ElementsAre(EndsWith(") Warning"), EndsWith(") Error")));

  // The minimum level only applies to the file.
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text, "Info"),
                          Field(&LogRecorder::Message::text, "Warning"),
                          Field(&LogRecorder::Message::text, "Error")));
}

TEST_F(LogFileTest, ReplacesExistingFile) {
  {
    LogFile log_file(path_, absl::LogSeverityAtLeast::kInfo);
    LOG(INFO) << "First";
  }
  {
    LogFile log_file(path_, absl::LogSeverityAtLeast::kInfo);
    LOG(INFO) << "Second";
  }

  EXPECT_THAT(ReadLines(), ElementsAre(EndsWith(") Second")));
}

TEST_F(LogFileTest, WritesWholeLinesFromManyThreads) {
  constexpr int kThreadCount = 8;
  constexpr int kMessageCount = 100;
  LogFile log_file(path_, absl::LogSeverityAtLeast::kInfo);
  ThreadTester tester;
  for (int thread = 0; thread < kThreadCount; ++thread) {
    tester.Run(absl::StrCat("Thread ", thread), [thread] {
      for (int i = 0; i < kMessageCount; ++i) {
        LOG(INFO) << "Thread " << thread << " message " << i;
      }
      return true;
    });
  }
  ASSERT_TRUE(tester.Complete());

  std::vector<std::string> expected_messages;
  for (int thread = 0; thread < kThreadCount; ++thread) {
    for (int i = 0; i < kMessageCount; ++i) {
      expected_messages.push_back(
          absl::StrCat("Thread ", thread, " message ", i));
    }
  }
  std::vector<std::string> messages;
  for (const std::string& line : ReadLines()) {
    EXPECT_THAT(line,
                MatchesRegex(absl::StrCat(kLinePrefixRegex,
                                          R"(\d+\) Thread \d+ message \d+)")));
    messages.push_back(line.substr(line.find(") ") + 2));
  }
  EXPECT_THAT(messages, UnorderedElementsAreArray(expected_messages));
}

TEST_F(LogFileTest, LogsErrorIfFileCantBeCreated) {
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / "missing" / "test.log";
  LogFile log_file(path, absl::LogSeverityAtLeast::kInfo);
  LOG(INFO) << "Hello";

  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text,
                                HasSubstr("Failed to create log file"))));
  EXPECT_FALSE(std::filesystem::exists(path));
}

}  // namespace
}  // namespace gb
