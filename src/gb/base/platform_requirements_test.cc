// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/platform_requirements.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(PlatformRequirementsTest, IsPlatformSupported) {
  EXPECT_TRUE(IsPlatformSupported());
}

}  // namespace
}  // namespace gb
