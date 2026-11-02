// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gtest/gtest.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb {
namespace {

// The Game Bits CMake commands embed CMake/GameBits.manifest in every
// executable, including this test.
TEST(WinManifestTest, CodePageIsUtf8) { EXPECT_EQ(GetACP(), CP_UTF8); }

}  // namespace
}  // namespace gb
