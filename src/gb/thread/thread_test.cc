// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/thread.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/notification.h"
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

struct SetAffinityState {
  uint64_t affinity = 0;
  bool set = false;
  uint64_t thread_affinity = 0;
};

// Pins the thread to the affinity in the SetAffinityState at `user_data`, and
// records the result.
void SetAffinity(void* user_data) {
  auto* state = static_cast<SetAffinityState*>(user_data);
  state->set = SetThisThreadAffinity(state->affinity);
  state->thread_affinity = GetThreadAffinity(GetThisThread());
}

TEST_F(ThreadTest, SetThisThreadAffinityPinsTheThread) {
  auto affinities = GetHardwareThreadAffinities();
  if (affinities.empty()) {
    GTEST_SKIP() << "Threads cannot be pinned on this platform";
  }
  SetAffinityState state = {.affinity = affinities.back()};
  Thread thread = CreateThread(0, 0, &state, SetAffinity);
  ASSERT_NE(thread, nullptr);
  JoinThread(thread);
  EXPECT_TRUE(state.set);
  EXPECT_EQ(state.thread_affinity, affinities.back());
}

TEST_F(ThreadTest, SetThisThreadAffinityFailsForZero) {
  SetAffinityState state;
  Thread thread = CreateThread(0, 0, &state, SetAffinity);
  ASSERT_NE(thread, nullptr);
  JoinThread(thread);
  EXPECT_FALSE(state.set);
  EXPECT_EQ(state.thread_affinity, 0);
}

TEST_F(ThreadTest, ThreadIsActiveUntilJoined) {
  Thread thread = CreateThread(0, 0, nullptr, +[](void* user_data) {});
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
  Thread thread = CreateThread(0, 0, nullptr, +[](void* user_data) {});
  ASSERT_NE(thread, nullptr);
  SetThreadName(thread, "Test");
  EXPECT_EQ(GetThreadName(thread), "Test");
  JoinThread(thread);
}

TEST_F(ThreadTest, LongThreadNameIsTrimmed) {
  Thread thread = CreateThread(0, 0, nullptr, +[](void* user_data) {});
  ASSERT_NE(thread, nullptr);
  SetThreadName(thread, std::string(200, 'a'));
  const std::string_view name = GetThreadName(thread);
  EXPECT_LT(name.size(), 200);
  EXPECT_EQ(name, std::string(name.size(), 'a'));
  JoinThread(thread);
}

TEST_F(ThreadTest, ThreadNameIsOnlyTheGivenText) {
  Thread thread = CreateThread(0, 0, nullptr, +[](void* user_data) {});
  ASSERT_NE(thread, nullptr);
  const std::string text = "Test" + std::string(200, 'b');
  SetThreadName(thread, std::string_view(text).substr(0, 4));
  EXPECT_EQ(GetThreadName(thread), std::string_view("Test"));
  JoinThread(thread);
}

TEST_F(ThreadTest, AccessNullThreadName) {
  SetThreadName(nullptr, "Test");
  EXPECT_EQ(GetThreadName(nullptr), "null");
}

TEST_F(ThreadTest, CoreClassIsNotNegative) {
  EXPECT_GE(GetThisThreadCoreClass(), 0);
}

// Enough work that the busy loop runs far longer than it takes to start and
// join a thread.
inline constexpr int kBusyIterations = 100'000'000;

// Runs a busy loop, and sets the int64_t at `user_data` to the cycles it took.
void RunBusyLoop(void* user_data) {
  const int64_t start = GetThisThreadCycles();
  volatile int counter = 0;
  for (int i = 0; i < kBusyIterations; ++i) {
    counter = counter + 1;
  }
  *static_cast<int64_t*>(user_data) = GetThisThreadCycles() - start;
}

TEST_F(ThreadTest, ThisThreadCyclesAreZeroWhenUnsupported) {
  if (GetThreadCapabilities().IsSet(ThreadCapability::kCycles)) {
    GTEST_SKIP() << "Thread cycles are supported on this platform";
  }
  EXPECT_EQ(GetThisThreadCycles(), 0);
}

TEST_F(ThreadTest, ThisThreadCyclesCountWorkButNotWaiting) {
  if (!GetThreadCapabilities().IsSet(ThreadCapability::kCycles)) {
    GTEST_SKIP() << "Thread cycles are not supported on this platform";
  }

  const int64_t start = GetThisThreadCycles();
  int64_t busy_cycles = 0;
  Thread thread = CreateThread(0, 0, &busy_cycles, RunBusyLoop);
  ASSERT_NE(thread, nullptr);
  JoinThread(thread);
  const int64_t waiting_cycles = GetThisThreadCycles() - start;

  EXPECT_GT(busy_cycles, 0);
  EXPECT_LT(waiting_cycles, busy_cycles / 10);
}

}  // namespace
}  // namespace gb
