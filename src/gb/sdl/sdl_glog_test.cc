// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/sdl/sdl_glog.h"

#include <string_view>
#include <type_traits>

#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;

Matcher<LogRecorder::Message> IsMessage(absl::LogSeverity severity,
                                        std::string_view text) {
  return AllOf(Field(&LogRecorder::Message::severity, severity),
               Field(&LogRecorder::Message::text, text));
}

Matcher<LogRecorder::Message> IsInfo(std::string_view text) {
  return IsMessage(absl::LogSeverity::kInfo, text);
}

static_assert(
    std::is_convertible_v<decltype(&SdlToGLog), SDL_LogOutputFunction>);

TEST(SdlGLogTest, Priorities) {
  LogRecorder log_recorder;
  SdlToGLog(nullptr, SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG,
            "debug");
  SdlToGLog(nullptr, SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO,
            "info");
  SdlToGLog(nullptr, SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN,
            "warn");
  SdlToGLog(nullptr, SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR,
            "error");
  EXPECT_THAT(
      log_recorder.TakeMessages(),
      ElementsAre(IsMessage(absl::LogSeverity::kInfo, "SDL(app) debug"),
                  IsMessage(absl::LogSeverity::kInfo, "SDL(app) info"),
                  IsMessage(absl::LogSeverity::kWarning, "SDL(app) warn"),
                  IsMessage(absl::LogSeverity::kError, "SDL(app) error")));
}

TEST(SdlGLogTest, VerboseIsVLogLevelOne) {
  LogRecorder log_recorder;
  SdlToGLog(nullptr, SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE,
            "hidden");
  EXPECT_THAT(log_recorder.TakeMessages(), IsEmpty());

  const int old_level = absl::SetGlobalVLogLevel(1);
  SdlToGLog(nullptr, SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE,
            "shown");
  absl::SetGlobalVLogLevel(old_level);
  EXPECT_THAT(
      log_recorder.TakeMessages(),
      ElementsAre(IsMessage(absl::LogSeverity::kInfo, "SDL(app) shown")));
}

TEST(SdlGLogTest, CriticalIsDebugFatal) {
  LogRecorder log_recorder;
  EXPECT_DEBUG_DEATH(SdlToGLog(nullptr, SDL_LOG_CATEGORY_APPLICATION,
                               SDL_LOG_PRIORITY_CRITICAL, "critical"),
                     "SDL\\(app\\) critical");
#ifdef NDEBUG
  EXPECT_THAT(
      log_recorder.TakeMessages(),
      ElementsAre(IsMessage(absl::LogSeverity::kError, "SDL(app) critical")));
#endif
}

TEST(SdlGLogTest, Categories) {
  const int kCategories[] = {
      SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_CATEGORY_ERROR,
      SDL_LOG_CATEGORY_ASSERT,      SDL_LOG_CATEGORY_SYSTEM,
      SDL_LOG_CATEGORY_AUDIO,       SDL_LOG_CATEGORY_VIDEO,
      SDL_LOG_CATEGORY_RENDER,      SDL_LOG_CATEGORY_INPUT,
      SDL_LOG_CATEGORY_TEST,        SDL_LOG_CATEGORY_RESERVED1,
      SDL_LOG_CATEGORY_CUSTOM,      SDL_LOG_CATEGORY_CUSTOM + 1};
  LogRecorder log_recorder;
  for (int category : kCategories) {
    SdlToGLog(nullptr, category, SDL_LOG_PRIORITY_INFO, "message");
  }
  EXPECT_THAT(
      log_recorder.TakeMessages(),
      ElementsAre(IsInfo("SDL(app) message"), IsInfo("SDL(error) message"),
                  IsInfo("SDL(assert) message"), IsInfo("SDL(system) message"),
                  IsInfo("SDL(audio) message"), IsInfo("SDL(video) message"),
                  IsInfo("SDL(render) message"), IsInfo("SDL(input) message"),
                  IsInfo("SDL(test) message"), IsInfo("SDL(other) message"),
                  IsInfo("SDL(other) message"), IsInfo("SDL(other) message")));
}

TEST(SdlGLogTest, UnknownPriorityIsIgnored) {
  LogRecorder log_recorder;
  SdlToGLog(nullptr, SDL_LOG_CATEGORY_APPLICATION, SDL_NUM_LOG_PRIORITIES,
            "unknown");
  SdlToGLog(nullptr, SDL_LOG_CATEGORY_APPLICATION,
            static_cast<SDL_LogPriority>(0), "zero");
  EXPECT_THAT(log_recorder.TakeMessages(), IsEmpty());
}

}  // namespace
}  // namespace gb
