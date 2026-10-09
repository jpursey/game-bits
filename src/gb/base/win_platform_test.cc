// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/win_platform.h"

#include <cstdint>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::StartsWith;

// Windows error codes, from winerror.h.
constexpr uint32_t kErrorFileNotFound = 2;
constexpr uint32_t kErrorWrongDisk = 34;  // Its message has inserts.
constexpr uint32_t kErrorNoMessage = 0x2000FFFF;

TEST(WinPlatformTest, GetWindowsErrorHasMessageAndCode) {
  const std::string message = GetWindowsError(kErrorFileNotFound);
  EXPECT_THAT(message, Not(StartsWith("no message")));
  EXPECT_THAT(message, EndsWith(" (2)"));
  EXPECT_THAT(message, Not(HasSubstr("\r")));
  EXPECT_THAT(message, Not(HasSubstr("\n")));
  EXPECT_THAT(message, Not(HasSubstr("  ")));
}

TEST(WinPlatformTest, GetWindowsErrorWithInserts) {
  const std::string message = GetWindowsError(kErrorWrongDisk);
  EXPECT_THAT(message, Not(StartsWith("no message")));
  EXPECT_THAT(message, EndsWith(" (34)"));
}

TEST(WinPlatformTest, GetWindowsErrorWithoutMessage) {
  EXPECT_EQ(GetWindowsError(kErrorNoMessage), "no message (536936447)");
}

}  // namespace
}  // namespace gb
