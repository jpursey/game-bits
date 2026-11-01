// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_assert.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(RenderAssertTest, EnabledOnlyWhenCompiled) {
  const bool was_enabled = IsRenderAssertEnabled();
  SetRenderAssertEnabled(true);
  EXPECT_EQ(IsRenderAssertEnabled(), kRenderAssertsCompiled);
  SetRenderAssertEnabled(false);
  EXPECT_FALSE(IsRenderAssertEnabled());
  SetRenderAssertEnabled(was_enabled);
}

TEST(RenderAssertTest, DisabledAssertIsNotEvaluated) {
  const bool was_enabled = IsRenderAssertEnabled();
  SetRenderAssertEnabled(false);
  int count = 0;
  RENDER_ASSERT(++count == 0);
  EXPECT_EQ(count, 0);
  SetRenderAssertEnabled(was_enabled);
}

TEST(RenderAssertTest, PassingAssertContinues) {
  const bool was_enabled = IsRenderAssertEnabled();
  SetRenderAssertEnabled(true);
  [[maybe_unused]] int count = 0;
  RENDER_ASSERT(++count == 1);
  EXPECT_EQ(count, kRenderAssertsCompiled ? 1 : 0);
  SetRenderAssertEnabled(was_enabled);
}

TEST(RenderAssertDeathTest, FailedAssertIsFatal) {
  if (!kRenderAssertsCompiled) {
    GTEST_SKIP() << "Render asserts are compiled out";
  }
  SetRenderAssertEnabled(true);
  EXPECT_DEATH(RENDER_ASSERT(1 == 2),
               "render_assert_test.cc.*Render assert failed: 1 == 2");
}

}  // namespace
}  // namespace gb
