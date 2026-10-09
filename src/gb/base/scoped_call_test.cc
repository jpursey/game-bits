// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/scoped_call.h"

#include <memory>
#include <type_traits>

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(ScopedCallTest, CallsOnDestruction) {
  int count = 0;
  {
    ScopedCall call([&count]() { ++count; });
    EXPECT_EQ(count, 0);
  }
  EXPECT_EQ(count, 1);
}

TEST(ScopedCallTest, CallsCopiedCallable) {
  int count = 0;
  const auto callback = [&count]() { ++count; };
  {
    ScopedCall call(callback);
  }
  EXPECT_EQ(count, 1);
}

TEST(ScopedCallTest, CallsMoveOnlyCallable) {
  int count = 0;
  {
    ScopedCall call(
        [&count, move_only = std::make_unique<int>()]() { ++count; });
  }
  EXPECT_EQ(count, 1);
}

// A copy would call the function a second time.
static_assert(!std::is_copy_constructible_v<ScopedCall<void (*)()>>);
static_assert(!std::is_copy_assignable_v<ScopedCall<void (*)()>>);

}  // namespace
}  // namespace gb
