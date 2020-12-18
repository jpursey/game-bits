// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/job_fiber.h"

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
  if (!SupportsJobFibers()) { \
    return;                   \
  }

void WaitAndDeleteJobFibers(absl::Span<const JobFiber> fibers) {
  while (GetRunningJobFiberCount() > 0) {
    std::this_thread::yield();
  }
  for (JobFiber fiber : fibers) {
    DeleteJobFiber(fiber);
  }
}

class JobFiberTest : public ::testing::Test {
 public:
  JobFiberTest() { SetJobFiberVerboseLogging(true); }
  ~JobFiberTest() override { SetJobFiberVerboseLogging(false); }
};

TEST_F(JobFiberTest, CreateMaxConcurrencyThreadCount) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateJobFiberThreads(
      0, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), GetMaxConcurrency());
  WaitAndDeleteJobFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(JobFiberTest, CreateMaxConcurrencyMinusOneThreadCount) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateJobFiberThreads(
      -1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), std::max(GetMaxConcurrency() - 1, 1));
  WaitAndDeleteJobFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(JobFiberTest, CreateMaxConcurrencyMinusAllThreadCount) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateJobFiberThreads(
      -GetMaxConcurrency(), false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), 1);
  WaitAndDeleteJobFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(JobFiberTest, CreateOneThreadCount) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateJobFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), 1);
  WaitAndDeleteJobFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(JobFiberTest, CreateMaxConcurrencyPlusOneThreadCount) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateJobFiberThreads(
      GetMaxConcurrency() + 1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), GetMaxConcurrency() + 1);
  WaitAndDeleteJobFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(JobFiberTest, CreateMaxConcurrencyThreadCountPinned) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateJobFiberThreads(
      0, true, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), GetMaxConcurrency());
  WaitAndDeleteJobFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(JobFiberTest, CreateMaxConcurrencyPlusOneThreadCountPinned) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateJobFiberThreads(
      GetMaxConcurrency() + 1, true, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), GetMaxConcurrency() + 1);
  WaitAndDeleteJobFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(JobFiberTest, CreateThreadsWithExplicitStackSize) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fibers = CreateJobFiberThreads(
      0, false, 32 * 1024, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  EXPECT_EQ(fibers.size(), GetMaxConcurrency());
  WaitAndDeleteJobFibers(fibers);
  EXPECT_EQ(state.counter, static_cast<int>(fibers.size()));
}

TEST_F(JobFiberTest, GetThisJobFiber) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<JobFiber> fiber = nullptr;
  } state;
  auto fibers = CreateJobFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber == nullptr) {
          std::this_thread::yield();
        }
        EXPECT_EQ(state.fiber, GetThisJobFiber());
      });
  ASSERT_EQ(fibers.size(), 1);
  state.fiber = fibers[0];
  WaitAndDeleteJobFibers(fibers);
}

TEST_F(JobFiberTest, CreateJobFiber) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fiber = CreateJobFiber(
      0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  ASSERT_NE(fiber, nullptr);
  absl::SleepFor(absl::Milliseconds(100));
  EXPECT_EQ(state.counter, 0);
  DeleteJobFiber(fiber);
}

TEST_F(JobFiberTest, CreateJobFiberWithExplicitStackSize) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fiber = CreateJobFiber(
      32 * 1024, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  ASSERT_NE(fiber, nullptr);
  absl::SleepFor(absl::Milliseconds(100));
  EXPECT_EQ(state.counter, 0);
  DeleteJobFiber(fiber);
}

TEST_F(JobFiberTest, SwitchToFiberAndExit) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
    std::atomic<JobFiber> fiber = nullptr;
  } state;
  auto fibers = CreateJobFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber == nullptr) {
          std::this_thread::yield();
        }
        SwitchToJobFiber(state.fiber);
        state.counter += 2;
      });
  state.fiber = CreateJobFiber(
      0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.counter += 1;
      });
  ASSERT_NE(state.fiber, nullptr);
  fibers.push_back(state.fiber);
  WaitAndDeleteJobFibers(fibers);
  EXPECT_EQ(state.counter, 1);
}

TEST_F(JobFiberTest, SwitchToFiberAndBackThenExit) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
    std::atomic<JobFiber> fiber = nullptr;
  } state;
  auto fibers = CreateJobFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber == nullptr) {
          std::this_thread::yield();
        }
        JobFiber next_fiber = state.fiber;
        state.fiber = GetThisJobFiber();
        SwitchToJobFiber(next_fiber);
        state.counter += 2;
      });
  auto new_fiber = CreateJobFiber(
      0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.counter += 1;
        SwitchToJobFiber(state.fiber);
      });
  ASSERT_NE(new_fiber, nullptr);
  state.fiber = new_fiber;
  fibers.push_back(new_fiber);
  WaitAndDeleteJobFibers(fibers);
  EXPECT_EQ(state.counter, 3);
}

TEST_F(JobFiberTest, SwapThreadsAndExit) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
    std::atomic<JobFiber> fiber_1 = nullptr;
    std::atomic<JobFiber> fiber_2 = nullptr;
    std::atomic<JobFiber> fiber_3 = nullptr;
  } state;
  auto fiber_1 = CreateJobFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber_1 == nullptr) {
          std::this_thread::yield();
        }
        JobFiber next_fiber = state.fiber_1;
        state.fiber_1 = GetThisJobFiber();
        state.counter += 1;
        SwitchToJobFiber(next_fiber);
        state.fiber_3 = state.fiber_2.load();
        state.counter += 8;
      });
  ASSERT_EQ(fiber_1.size(), 1);
  auto fiber_2 = CreateJobFiberThreads(
      1, false, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber_2 == nullptr) {
          std::this_thread::yield();
        }
        JobFiber next_fiber = state.fiber_2;
        state.fiber_2 = GetThisJobFiber();
        state.counter += 2;
        SwitchToJobFiber(next_fiber);
        state.counter += 16;
      });
  ASSERT_EQ(fiber_2.size(), 1);
  auto fiber_3 = CreateJobFiber(
      0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.fiber_2 = state.fiber_1.load();
        while (state.fiber_3 == nullptr) {
          std::this_thread::yield();
        }
        JobFiber next_fiber = state.fiber_3;
        state.counter += 4;
        SwitchToJobFiber(next_fiber);
      });
  state.fiber_1 = fiber_3;
  WaitAndDeleteJobFibers({fiber_1[0], fiber_2[0], fiber_3});
  EXPECT_EQ(state.counter, 31);
}

TEST_F(JobFiberTest, ThreadAbuse) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;

    JobFiberMain callback;
    std::atomic<int> counter = 0;

    absl::Mutex mutex;
    Queue<JobFiber> idle_fibers{100} ABSL_GUARDED_BY(mutex);
    Queue<JobFiber> fibers_to_idle{100} ABSL_GUARDED_BY(mutex);
    std::vector<JobFiber> all_fibers ABSL_GUARDED_BY(mutex);
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
        JobFiber fiber = CreateJobFiber(4096, &state, state.callback);
        state.mutex.Lock();
        state.all_fibers.push_back(fiber);
        state.idle_fibers.push(fiber);
        state.mutex.Unlock();
      }
      state.mutex.Lock();
      if (!state.fibers_to_idle.empty()) {
        JobFiber maybe_idle = state.fibers_to_idle.front();
        if (!IsJobFiberRunning(maybe_idle)) {
          state.fibers_to_idle.pop();
          state.idle_fibers.push(maybe_idle);
        }
      }
      JobFiber next_fiber = nullptr;
      if (!done) {
        state.mutex.Await(absl::Condition(
            +[](State* state) { return !state->idle_fibers.empty(); }, &state));
        next_fiber = state.idle_fibers.front();
        state.idle_fibers.pop();
      }
      state.fibers_to_idle.push(GetThisJobFiber());
      state.mutex.Unlock();

      if (next_fiber != nullptr) {
        SwitchToJobFiber(next_fiber);
      }
    }
  };

  state.mutex.Lock();
  const int num_threads = std::max(4, GetMaxConcurrency());
  state.all_fibers =
      CreateJobFiberThreads(num_threads, true, 4096, &state, state.callback);
  for (int i = 0; i < 5; ++i) {
    JobFiber fiber = CreateJobFiber(4096, &state, state.callback);
    state.all_fibers.push_back(fiber);
    state.idle_fibers.push(fiber);
  }
  state.mutex.Unlock();

  while (GetRunningJobFiberCount() > 0) {
    std::this_thread::yield();
  }

  state.mutex.Lock();
  EXPECT_EQ(state.counter, 1000 + num_threads);
  WaitAndDeleteJobFibers(state.all_fibers);
  state.mutex.Unlock();
}

}  // namespace
}  // namespace gb
