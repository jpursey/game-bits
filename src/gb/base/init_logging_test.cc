// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/init_logging.h"

#include "absl/log/log.h"
#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;

TEST(InitLoggingTest, CanBeCalledMoreThanOnce) {
  InitLogging();
  InitLogging();

  LogRecorder log_recorder;
  LOG(INFO) << "Logged";
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text, "Logged")));
}

}  // namespace
}  // namespace gb
