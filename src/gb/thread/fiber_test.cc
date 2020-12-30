// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/fiber.h"

#include <thread>

#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/types/span.h"
#include "gb/base/queue.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

#define CHECK_FIBER_SUPPORT() \
  if (!SupportsFibers()) {    \
    return;                   \
  }

void WaitAndDeleteFibers(absl::Span<const Fiber> fibers) {
  while (GetRunningFiberCount() > 0) {
    std::this_thread::yield();
  }
  for (Fiber fiber : fibers) {
    DeleteFiber(fiber);
  }
}

class FiberTest : public ::testing::Test {
 public:
  FiberTest() { SetFiberVerboseLogging(true); }
  ~FiberTest() override { SetFiberVerboseLogging(false); }
};

TEST_F(FiberTest, CreateMaxConcurrencyThreadCount) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateFiberThreads(
      0, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), GetMaxConcurrency());
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(FiberTest, CreateMaxConcurrencyMinusOneThreadCount) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateFiberThreads(
      -1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), std::max(GetMaxConcurrency() - 1, 1));
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(FiberTest, CreateMaxConcurrencyMinusAllThreadCount) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateFiberThreads(
      -GetMaxConcurrency(), false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), 1);
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(FiberTest, CreateOneThreadCount) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), 1);
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(FiberTest, CreateMaxConcurrencyPlusOneThreadCount) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateFiberThreads(
      GetMaxConcurrency() + 1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), GetMaxConcurrency() + 1);
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(FiberTest, CreateMaxConcurrencyThreadCountPinned) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateFiberThreads(
      0, true, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), GetMaxConcurrency());
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(FiberTest, CreateMaxConcurrencyPlusOneThreadCountPinned) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateFiberThreads(
      GetMaxConcurrency() + 1, true, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), GetMaxConcurrency() + 1);
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(FiberTest, CreateThreadsWithExplicitStackSize) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateFiberThreads(
      0, false, 32 * 1024, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), GetMaxConcurrency());
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(FiberTest, GetThisFiber) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<Fiber> fiber = nullptr;
  } state;
  auto fibers = CreateFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber == nullptr) {
          std::this_thread::yield();
        }
        EXPECT_EQ(state.fiber, GetThisFiber());
      });
  ASSERT_EQ(fibers.size(), 1);
  state.fiber = fibers[0];
  WaitAndDeleteFibers(fibers);
}

TEST_F(FiberTest, CreateFiber) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fiber = CreateFiber(
      0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  ASSERT_NE(fiber, nullptr);
  absl::SleepFor(absl::Milliseconds(100));
  EXPECT_EQ(state.counter, 0);
  DeleteFiber(fiber);
}

TEST_F(FiberTest, CreateFiberWithExplicitStackSize) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fiber = CreateFiber(
      32 * 1024, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  ASSERT_NE(fiber, nullptr);
  absl::SleepFor(absl::Milliseconds(100));
  EXPECT_EQ(state.counter, 0);
  DeleteFiber(fiber);
}

TEST_F(FiberTest, SwitchToFiberAndExit) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
    std::atomic<Fiber> fiber = nullptr;
  } state;
  auto fibers = CreateFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber == nullptr) {
          std::this_thread::yield();
        }
        SwitchToFiber(state.fiber);
        state.counter += 2;
      });
  state.fiber = CreateFiber(
      0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.counter += 1;
      });
  ASSERT_NE(state.fiber, nullptr);
  fibers.push_back(state.fiber);
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(state.counter, 1);
}

TEST_F(FiberTest, SwitchToFiberAndBackThenExit) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
    std::atomic<Fiber> fiber = nullptr;
  } state;
  auto fibers = CreateFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber == nullptr) {
          std::this_thread::yield();
        }
        Fiber next_fiber = state.fiber;
        state.fiber = GetThisFiber();
        SwitchToFiber(next_fiber);
        state.counter += 2;
      });
  auto new_fiber = CreateFiber(
      0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.counter += 1;
        SwitchToFiber(state.fiber);
      });
  ASSERT_NE(new_fiber, nullptr);
  state.fiber = new_fiber;
  fibers.push_back(new_fiber);
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(state.counter, 3);
}

TEST_F(FiberTest, SwapThreadsAndExit) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
    std::atomic<Fiber> fiber_1 = nullptr;
    std::atomic<Fiber> fiber_2 = nullptr;
    std::atomic<Fiber> fiber_3 = nullptr;
  } state;
  auto fiber_1 = CreateFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber_1 == nullptr) {
          std::this_thread::yield();
        }
        Fiber next_fiber = state.fiber_1;
        state.fiber_1 = GetThisFiber();
        state.counter += 1;
        SwitchToFiber(next_fiber);
        state.fiber_3 = state.fiber_2.load();
        state.counter += 8;
      });
  ASSERT_EQ(fiber_1.size(), 1);
  auto fiber_2 = CreateFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber_2 == nullptr) {
          std::this_thread::yield();
        }
        Fiber next_fiber = state.fiber_2;
        state.fiber_2 = GetThisFiber();
        state.counter += 2;
        SwitchToFiber(next_fiber);
        state.counter += 16;
      });
  ASSERT_EQ(fiber_2.size(), 1);
  auto fiber_3 = CreateFiber(
      0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.fiber_2 = state.fiber_1.load();
        while (state.fiber_3 == nullptr) {
          std::this_thread::yield();
        }
        Fiber next_fiber = state.fiber_3;
        state.counter += 4;
        SwitchToFiber(next_fiber);
      });
  state.fiber_1 = fiber_3;
  WaitAndDeleteFibers({fiber_1[0], fiber_2[0], fiber_3});
  EXPECT_EQ(state.counter, 31);
}

TEST_F(FiberTest, ThreadAbuse) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;

    FiberMain callback;
    std::atomic<int> counter = 0;

    absl::Mutex mutex;
    Queue<Fiber> idle_fibers{100} ABSL_GUARDED_BY(mutex);
    Queue<Fiber> fibers_to_idle{100} ABSL_GUARDED_BY(mutex);
    std::vector<Fiber> all_fibers ABSL_GUARDED_BY(mutex);
  } state;

  state.callback = +[](void* user_data) {
    auto& state = *static_cast<State*>(user_data);

    bool done = false;
    while (!done) {
      int count = ++state.counter;
      if (count > 1000) {
        done = true;
      }
      if (count % 50 == 0) {
        Fiber fiber = CreateFiber(4096, &state, state.callback);
        state.mutex.Lock();
        state.all_fibers.push_back(fiber);
        state.idle_fibers.push(fiber);
        state.mutex.Unlock();
      }
      state.mutex.Lock();
      if (!state.fibers_to_idle.empty()) {
        Fiber maybe_idle = state.fibers_to_idle.front();
        if (!IsFiberRunning(maybe_idle)) {
          state.fibers_to_idle.pop();
          state.idle_fibers.push(maybe_idle);
        }
      }
      Fiber next_fiber = nullptr;
      if (!done) {
        state.mutex.Await(absl::Condition(
            +[](State* state) { return !state->idle_fibers.empty(); }, &state));
        next_fiber = state.idle_fibers.front();
        state.idle_fibers.pop();
      }
      state.fibers_to_idle.push(GetThisFiber());
      state.mutex.Unlock();

      if (next_fiber != nullptr) {
        SwitchToFiber(next_fiber);
      }
    }
  };

  state.mutex.Lock();
  const int num_threads = std::max(4, GetMaxConcurrency());
  state.all_fibers =
      CreateFiberThreads(num_threads, true, 4096, &state, state.callback);
  for (int i = 0; i < 5; ++i) {
    Fiber fiber = CreateFiber(4096, &state, state.callback);
    state.all_fibers.push_back(fiber);
    state.idle_fibers.push(fiber);
  }
  state.mutex.Unlock();

  while (GetRunningFiberCount() > 0) {
    std::this_thread::yield();
  }

  state.mutex.Lock();
  EXPECT_EQ(state.counter, 1000 + num_threads);
  WaitAndDeleteFibers(state.all_fibers);
  state.mutex.Unlock();
}

}  // namespace
}  // namespace gb
