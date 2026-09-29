// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profile_call_hook.h"

#include "absl/time/time.h"
#include "gb/base/function_hook.h"
#include "gb/profile/fake_ticks.h"
#include "gb/profile/profile_timer.h"
#include "gb/profile/profiler.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// Points are registered for the whole program, so each test uses names of its
// own. Every tick is a nanosecond.

// The ticks the functions below advance, as the other system's work. Tests
// only check how far they advance.
FakeTicks g_ticks;

int Add(int a, int b) {
  g_ticks.Advance(10);
  return a + b;
}

// Does some work, then calls back into the program, which does its own.
void CallBack() {
  g_ticks.Advance(10);
  ProfileScope<"CallHookTest/Callback"> scope;
  g_ticks.Advance(4);
}

int (*g_add)(int, int) = &Add;
void (*g_call_back)() = &CallBack;

class ProfileCallHookTest : public ::testing::Test {
 protected:
  Profiler profiler_{{.fake_ticks = &g_ticks}};
};

TEST_F(ProfileCallHookTest, TimesCallsThroughPointer) {
  FunctionHook<&g_add, ProfileCallHook> hook("CallHookTest/Add");
  EXPECT_EQ(g_add(1, 2), 3);
  EXPECT_EQ(g_add(3, 4), 7);
  EXPECT_EQ(profiler_.GetCount("CallHookTest/Add"), 2);
  EXPECT_EQ(profiler_.GetSelfTime("CallHookTest/Add"), absl::Nanoseconds(20));
}

TEST_F(ProfileCallHookTest, CallbackIsSubtracted) {
  FunctionHook<&g_call_back, ProfileCallHook> hook("CallHookTest/CallBack");
  g_call_back();
  EXPECT_EQ(profiler_.GetSelfTime("CallHookTest/CallBack"),
            absl::Nanoseconds(10));
  EXPECT_EQ(profiler_.GetSelfTime("CallHookTest/Callback"),
            absl::Nanoseconds(4));
}

TEST(ProfileCallHookNoProfilerTest, CallsThrough) {
  FunctionHook<&g_add, ProfileCallHook> hook("CallHookTest/NoProfiler");
  EXPECT_EQ(g_add(1, 2), 3);
}

}  // namespace
}  // namespace gb
