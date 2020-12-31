// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/thread.h"

#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/notification.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class ThreadTest : public ::testing::Test {
 public:
  ThreadTest() { SetThreadVerboseLogging(true); }
  ~ThreadTest() override { SetThreadVerboseLogging(false); }
};

TEST_F(ThreadTest, GetMaxConcurrencyIsGreaterThanZero) {
  EXPECT_GT(GetMaxConcurrency(), 0);
}

TEST_F(ThreadTest, ThreadAffinitiesAreUniqueAndNonZero) {
  auto affinities = GetHardwareThreadAffinities();
  absl::flat_hash_map<uint64_t, int> found;
  for (int i = 0; i < static_cast<int>(affinities.size()); ++i) {
    EXPECT_NE(affinities[i], 0) << "i=" << i;
    auto it = found.find(affinities[i]);
    if (it != found.end()) {
      FAIL() << "Affinity " << affinities[i] << " at both location " << i
             << " and " << it->second;
    }
    found[affinities[i]] = i;
  }
}

TEST_F(ThreadTest, CreateThreadWithNoAffinityNoStackSize) {
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  Thread thread = CreateThread(
      0, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  ASSERT_NE(thread, nullptr);
  EXPECT_EQ(GetThreadAffinity(thread), 0);
  JoinThread(thread);
  EXPECT_EQ(state.counter, 1);
}

TEST_F(ThreadTest, CreateThreadWithAffinityNoStackSize) {
  auto affinities = GetHardwareThreadAffinities();
  if (affinities.empty()) {
    return;
  }
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  Thread thread = CreateThread(
      affinities[0], 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  ASSERT_NE(thread, nullptr);
  EXPECT_EQ(GetThreadAffinity(thread), affinities[0]);
  JoinThread(thread);
  EXPECT_EQ(state.counter, 1);
}

TEST_F(ThreadTest, CreateThreadWithNoAffinityWithStackSize) {
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  Thread thread = CreateThread(
      0, 4096, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  ASSERT_NE(thread, nullptr);
  EXPECT_EQ(GetThreadAffinity(thread), 0);
  JoinThread(thread);
  EXPECT_EQ(state.counter, 1);
}

TEST_F(ThreadTest, CreateThreadWithAffinityWithStackSize) {
  auto affinities = GetHardwareThreadAffinities();
  if (affinities.empty()) {
    return;
  }
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  Thread thread = CreateThread(
      affinities[0], 4096, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  ASSERT_NE(thread, nullptr);
  EXPECT_EQ(GetThreadAffinity(thread), affinities[0]);
  JoinThread(thread);
  EXPECT_EQ(state.counter, 1);
}

TEST_F(ThreadTest, ThreadIsActiveUntilJoined) {
  Thread thread = CreateThread(
      0, 0, nullptr, +[](void* user_data) {});
  ASSERT_NE(thread, nullptr);
  EXPECT_EQ(GetActiveThreadCount(), 1);
  JoinThread(thread);
  EXPECT_EQ(GetActiveThreadCount(), 0);
}

TEST_F(ThreadTest, DetachThreadRemainsActiveUntilNotRunning) {
  struct State {
    State() = default;
    absl::Notification thread_started;
    absl::Notification end_thread;
    std::atomic<int> counter = 0;
  } state;
  Thread thread = CreateThread(
      0, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.thread_started.Notify();
        state.end_thread.WaitForNotification();
        ++state.counter;
      });
  ASSERT_NE(thread, nullptr);
  EXPECT_EQ(GetActiveThreadCount(), 1);
  DetachThread(thread);
  state.thread_started.WaitForNotification();
  EXPECT_EQ(GetActiveThreadCount(), 1);
  state.end_thread.Notify();
  while (GetActiveThreadCount() > 0) {
    std::this_thread::yield();
  }
  EXPECT_EQ(state.counter, 1);
}

TEST_F(ThreadTest, GetThisThreadIsNullForMainThread) {
  EXPECT_EQ(GetThisThread(), nullptr);
}

TEST_F(ThreadTest, GetThisThreadWorksInThread) {
  struct State {
    State() = default;
    std::atomic<Thread> thread = nullptr;
  } state;
  Thread thread = CreateThread(
      0, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.thread = GetThisThread();
      });
  ASSERT_NE(thread, nullptr);
  JoinThread(thread);
  EXPECT_EQ(state.thread, thread);
}

TEST_F(ThreadTest, ThreadName) {
  Thread thread = CreateThread(
      0, 0, nullptr, +[](void* user_data) {});
  ASSERT_NE(thread, nullptr);
  SetThreadName(thread, "Test");
  EXPECT_EQ(GetThreadName(thread), "Test");
  JoinThread(thread);
}

TEST_F(ThreadTest, AccessNullThreadName) {
  SetThreadName(nullptr, "Test");
  EXPECT_EQ(GetThreadName(nullptr), "null");
}

}  // namespace
}  // namespace gb
