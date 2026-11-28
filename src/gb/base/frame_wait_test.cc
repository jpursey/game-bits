// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/frame_wait.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

// Tests run as programs (under Node on the web), never in a browser page, so
// neither function waits here. Waiting in a page is checked with one.

TEST(FrameWaitTest, WaitForNextFrameReturnsFalseOutsidePage) {
  EXPECT_FALSE(WaitForNextFrame());
}

TEST(FrameWaitTest, YieldBetweenFramesReturnsOutsidePage) {
  YieldBetweenFrames();
}

}  // namespace
}  // namespace gb
