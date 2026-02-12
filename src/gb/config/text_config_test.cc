// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/config/text_config.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(WriteConfigToTextTest, Bool) {
  EXPECT_EQ(WriteConfigToText(Config::Bool(true)), "true");
  EXPECT_EQ(WriteConfigToText(Config::Bool(false)), "false");
}

}  // namespace
}  // namespace gb
