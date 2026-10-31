// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/fiber.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/cleanup/cleanup.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/types/span.h"
#include "gb/container/queue.h"
#include "gb/thread/thread.h"
#include "gb/thread/thread_placement.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

#define CHECK_FIBER_SUPPORT() \
  if (!SupportsFibers()) {    \
    return;                   \
  }

void WaitAndDeleteFibers(absl::Span<const FiberThread> fiber_threads) {
  for (const auto& fiber_thread : fiber_threads) {
    JoinThread(fiber_thread.thread);
  }
  for (const auto& fiber_thread : fiber_threads) {
    DeleteFiber(fiber_thread.fiber);
  }
}

// Fiber main that counts the fibers run, in the std::atomic<int> it is given.
void CountFiber(void* user_data) {
  ++*static_cast<std::atomic<int>*>(user_data);
}

// Creates a fiber thread for each of `threads`, and expects each to run, with
// its own affinity.
void ExpectCreatesThreads(absl::Span<const PlacedThread> threads) {
  std::atomic<int> counter = 0;
  auto fibers = CreateFiberThreads(threads, {}, 0, &counter, CountFiber);
  ASSERT_EQ(fibers.size(), threads.size());
  for (int i = 0; i < static_cast<int>(fibers.size()); ++i) {
    EXPECT_EQ(GetThreadAffinity(fibers[i].thread), threads[i].affinity) << i;
  }
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(counter, static_cast<int>(fibers.size()));
}

class FiberTest : public ::testing::Test {
 public:
  FiberTest() { SetFiberVerboseLogging(true); }
  ~FiberTest() override { SetFiberVerboseLogging(false); }
};

TEST_F(FiberTest, CreateNoThreads) {
  CHECK_FIBER_SUPPORT();
  ExpectCreatesThreads({});
}

TEST_F(FiberTest, CreateUnpinnedThreads) {
  CHECK_FIBER_SUPPORT();
  ExpectCreatesThreads(std::vector<PlacedThread>(3));
}

TEST_F(FiberTest, CreateThreadsPinnedToHardwareThreads) {
  CHECK_FIBER_SUPPORT();
  const ThreadPlacement placement = PlaceThreads({});
  if (placement.threads[0].affinity == 0) {
    GTEST_SKIP() << "Threads cannot be pinned";
  }
  ExpectCreatesThreads(placement.threads);
}

TEST_F(FiberTest, CreateThreadsPinnedToCores) {
  CHECK_FIBER_SUPPORT();
  const ThreadPlacement placement =
      PlaceThreads({.pinning = ThreadPinning::kCore});
  if (placement.threads[0].affinity == 0) {
    GTEST_SKIP() << "Threads cannot be pinned";
  }
  ExpectCreatesThreads(placement.threads);
}

TEST_F(FiberTest, CreateThreadsWithExplicitStackSize) {
  CHECK_FIBER_SUPPORT();
  std::atomic<int> counter = 0;
  auto fibers =
      CreateFiberThreads(std::vector<PlacedThread>(GetMaxConcurrency()), {},
                         32 * 1024, &counter, CountFiber);
  EXPECT_EQ(fibers.size(), GetMaxConcurrency());
  WaitAndDeleteFibers(fibers);
  EXPECT_EQ(counter, static_cast<int>(fibers.size()));
}

TEST_F(FiberTest, GetThisFiber) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<Fiber> fiber = nullptr;
  } state;
  auto fibers = CreateFiberThreads(
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber == nullptr) {
          std::this_thread::yield();
        }
        EXPECT_EQ(state.fiber, GetThisFiber());
      });
  ASSERT_EQ(fibers.size(), 1);
  state.fiber = fibers[0].fiber;
  WaitAndDeleteFibers(fibers);
}

TEST_F(FiberTest, CreateFiber) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    std::atomic<int> counter = 0;
  } state;
  auto fiber = CreateFiber(
      {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  ASSERT_NE(fiber, nullptr);
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
      {}, 32 * 1024, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        ++state.counter;
      });
  ASSERT_NE(fiber, nullptr);
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
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        while (state.fiber == nullptr) {
          std::this_thread::yield();
        }
        SwitchToFiber(state.fiber);
        state.counter += 2;
      });
  state.fiber = CreateFiber(
      {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.counter += 1;
      });
  ASSERT_NE(state.fiber, nullptr);
  WaitAndDeleteFibers(fibers);
  DeleteFiber(state.fiber);
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
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
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
      {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.counter += 1;
        SwitchToFiber(state.fiber);
      });
  ASSERT_NE(new_fiber, nullptr);
  state.fiber = new_fiber;
  WaitAndDeleteFibers(fibers);
  DeleteFiber(new_fiber);
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
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
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
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
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
      {}, 0, &state, +[](void* user_data) {
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
  WaitAndDeleteFibers({fiber_1[0], fiber_2[0]});
  DeleteFiber(fiber_3);
  EXPECT_EQ(state.counter, 31);
}

// The threads a fiber moved between in MoveFiberBetweenThreads().
struct FiberMove {
  // The two threads, in the order the fiber ran on them.
  Thread threads[2] = {};

  // What GetThisThread() returned on the fiber on each of them.
  Thread seen_threads[2] = {};
};

// Runs a fiber, named "M", on a new thread, whose own fiber is named "T1", and
// switches back to it. Then it resumes the fiber on a second new thread, "T2",
// while the first is still alive, and switches back to that. The fiber reads
// its thread on the first, and then in the same function after resuming on the
// second, so a thread local address kept from before the switch would read the
// first.
void MoveFiberBetweenThreads(FiberMove& move) {
  struct State {
    FiberMove* move = nullptr;
    Fiber moving_fiber = nullptr;
    Fiber return_fiber = nullptr;
    absl::Notification first_visit_done;
    absl::Notification second_visit_done;
  } state;
  state.move = &move;
  state.moving_fiber = CreateFiber(
      {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.move->seen_threads[0] = GetThisThread();
        SwitchToFiber(state.return_fiber);
        state.move->seen_threads[1] = GetThisThread();
        SwitchToFiber(state.return_fiber);
      });
  ASSERT_NE(state.moving_fiber, nullptr);
  SetFiberName(state.moving_fiber, "M");
  absl::Cleanup delete_moving_fiber = [&state] {
    DeleteFiber(state.moving_fiber);
  };
  auto fiber_1 = CreateFiberThreads(
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.return_fiber = GetThisFiber();
        SetFiberName(state.return_fiber, "T1");
        EXPECT_TRUE(SwitchToFiber(state.moving_fiber));
        state.first_visit_done.Notify();
        state.second_visit_done.WaitForNotification();
      });
  ASSERT_EQ(fiber_1.size(), 1);
  move.threads[0] = fiber_1[0].thread;

  // The first thread waits for the second to finish, so if the second can't be
  // created, this releases the first before it is joined.
  absl::Cleanup end_fiber_1 = [&state, &fiber_1] {
    if (!state.second_visit_done.HasBeenNotified()) {
      state.second_visit_done.Notify();
    }
    WaitAndDeleteFibers(fiber_1);
  };
  auto fiber_2 = CreateFiberThreads(
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.first_visit_done.WaitForNotification();
        state.return_fiber = GetThisFiber();
        SetFiberName(state.return_fiber, "T2");
        EXPECT_TRUE(SwitchToFiber(state.moving_fiber));
        state.second_visit_done.Notify();
      });
  ASSERT_EQ(fiber_2.size(), 1);
  move.threads[1] = fiber_2[0].thread;
  WaitAndDeleteFibers(fiber_2);
}

TEST_F(FiberTest, GetThisThreadAfterFiberMovesThreads) {
  CHECK_FIBER_SUPPORT();
  FiberMove move;
  ASSERT_NO_FATAL_FAILURE(MoveFiberBetweenThreads(move));
  EXPECT_EQ(move.seen_threads[0], move.threads[0]);
  EXPECT_EQ(move.seen_threads[1], move.threads[1]);
}

//------------------------------------------------------------------------------
// Fiber switch callbacks
//------------------------------------------------------------------------------

// A call to one of the recording switch callbacks below.
struct SwitchCall {
  // "suspend" or "resume", the fiber's name, and the state on_suspend returned
  // or on_resume was passed, such as "suspend A 1".
  std::string text;
  Thread thread = nullptr;
};

ABSL_CONST_INIT absl::Mutex g_switch_calls_mutex(absl::kConstInit);
std::vector<SwitchCall> g_switch_calls ABSL_GUARDED_BY(g_switch_calls_mutex);
int g_suspend_count ABSL_GUARDED_BY(g_switch_calls_mutex) = 0;

void ClearSwitchCalls() {
  absl::MutexLock lock(&g_switch_calls_mutex);
  g_switch_calls.clear();
  g_suspend_count = 0;
}

// Returns the number of the suspend, counting from 1, as the state.
void* RecordSuspend() {
  absl::MutexLock lock(&g_switch_calls_mutex);
  const int number = ++g_suspend_count;
  g_switch_calls.push_back(
      {absl::StrCat("suspend ", GetFiberName(GetThisFiber()), " ", number),
       GetThisThread()});
  return reinterpret_cast<void*>(static_cast<intptr_t>(number));
}

void RecordResume(void* state) {
  absl::MutexLock lock(&g_switch_calls_mutex);
  g_switch_calls.push_back(
      {absl::StrCat("resume ", GetFiberName(GetThisFiber()), " ",
                    reinterpret_cast<intptr_t>(state)),
       GetThisThread()});
}

constexpr FiberSwitchCallbacks kRecordSwitches = {
    .on_suspend = RecordSuspend,
    .on_resume = RecordResume,
};

std::vector<std::string> GetSwitchCallTexts() {
  absl::MutexLock lock(&g_switch_calls_mutex);
  std::vector<std::string> texts;
  for (const SwitchCall& call : g_switch_calls) {
    texts.push_back(call.text);
  }
  return texts;
}

class FiberSwitchCallbacksTest : public FiberTest {
 public:
  FiberSwitchCallbacksTest() {
    ClearSwitchCalls();
    SetFiberSwitchCallbacks(&kRecordSwitches);
  }
  ~FiberSwitchCallbacksTest() override { SetFiberSwitchCallbacks(nullptr); }
};

TEST_F(FiberSwitchCallbacksTest, EachSwitchSuspendsAndResumesItsFiber) {
  CHECK_FIBER_SUPPORT();
  struct State {
    Fiber thread_fiber = nullptr;
    Fiber fiber_b = nullptr;
  } state;
  state.fiber_b = CreateFiber(
      {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        SwitchToFiber(state.thread_fiber);
      });
  ASSERT_NE(state.fiber_b, nullptr);
  SetFiberName(state.fiber_b, "B");
  auto fibers = CreateFiberThreads(
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.thread_fiber = GetThisFiber();
        SetFiberName(state.thread_fiber, "A");
        EXPECT_TRUE(SwitchToFiber(state.fiber_b));
      });
  ASSERT_EQ(fibers.size(), 1);
  WaitAndDeleteFibers(fibers);
  DeleteFiber(state.fiber_b);

  // B starts in its main, so it has no resume, and it never resumes after its
  // switch back.
  EXPECT_THAT(GetSwitchCallTexts(),
              ElementsAre("suspend A 1", "suspend B 2", "resume A 1"));
}

TEST_F(FiberSwitchCallbacksTest, ResumesWithStateOnAnotherThread) {
  CHECK_FIBER_SUPPORT();
  FiberMove move;
  ASSERT_NO_FATAL_FAILURE(MoveFiberBetweenThreads(move));

  // M suspends on the first thread, as suspend 2, and resumes with that state
  // on the second.
  EXPECT_THAT(
      GetSwitchCallTexts(),
      ElementsAre("suspend T1 1", "suspend M 2", "resume T1 1", "suspend T2 3",
                  "resume M 2", "suspend M 4", "resume T2 3"));
  absl::MutexLock lock(&g_switch_calls_mutex);
  ASSERT_EQ(g_switch_calls.size(), 7);
  EXPECT_EQ(g_switch_calls[1].thread, move.threads[0]);
  EXPECT_EQ(g_switch_calls[4].thread, move.threads[1]);
}

TEST_F(FiberSwitchCallbacksTest, FailedSwitchCallsNeither) {
  CHECK_FIBER_SUPPORT();
  Fiber fiber = CreateFiber({}, 0, nullptr, +[](void* user_data) {});
  ASSERT_NE(fiber, nullptr);

  // This thread isn't a fiber, and a fiber can't switch to itself.
  EXPECT_FALSE(SwitchToFiber(fiber));
  auto fibers = CreateFiberThreads(
      std::vector<PlacedThread>(1), {}, 0, nullptr,
      +[](void* user_data) { EXPECT_FALSE(SwitchToFiber(GetThisFiber())); });
  ASSERT_EQ(fibers.size(), 1);
  WaitAndDeleteFibers(fibers);
  DeleteFiber(fiber);
  EXPECT_THAT(GetSwitchCallTexts(), IsEmpty());
}

TEST_F(FiberSwitchCallbacksTest, ResumesWithCallbacksItSuspendedWith) {
  CHECK_FIBER_SUPPORT();
  struct State {
    Fiber thread_fiber = nullptr;
    Fiber fiber_b = nullptr;
  } state;

  // B clears the callbacks while A is suspended, so A still resumes with them,
  // and B suspends with none. A sets them again, and B, which suspended with
  // none, resumes with none.
  state.fiber_b = CreateFiber(
      {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        SetFiberSwitchCallbacks(nullptr);
        SwitchToFiber(state.thread_fiber);
      });
  ASSERT_NE(state.fiber_b, nullptr);
  SetFiberName(state.fiber_b, "B");
  auto fibers = CreateFiberThreads(
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.thread_fiber = GetThisFiber();
        SetFiberName(state.thread_fiber, "A");
        EXPECT_TRUE(SwitchToFiber(state.fiber_b));
        SetFiberSwitchCallbacks(&kRecordSwitches);
        EXPECT_TRUE(SwitchToFiber(state.fiber_b));
      });
  ASSERT_EQ(fibers.size(), 1);
  WaitAndDeleteFibers(fibers);
  DeleteFiber(state.fiber_b);
  EXPECT_THAT(GetSwitchCallTexts(),
              ElementsAre("suspend A 1", "resume A 1", "suspend A 2"));
}

void* IgnoreSuspend() { return nullptr; }
void IgnoreResume(void* state) {}

TEST_F(FiberSwitchCallbacksTest, SettingSameSetAgainKeepsIt) {
  CHECK_FIBER_SUPPORT();
  SetFiberSwitchCallbacks(&kRecordSwitches);
  struct State {
    Fiber thread_fiber = nullptr;
    Fiber fiber_b = nullptr;
  } state;
  state.fiber_b = CreateFiber(
      {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        SwitchToFiber(state.thread_fiber);
      });
  ASSERT_NE(state.fiber_b, nullptr);
  SetFiberName(state.fiber_b, "B");
  auto fibers = CreateFiberThreads(
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        state.thread_fiber = GetThisFiber();
        SetFiberName(state.thread_fiber, "A");
        EXPECT_TRUE(SwitchToFiber(state.fiber_b));
      });
  ASSERT_EQ(fibers.size(), 1);
  WaitAndDeleteFibers(fibers);
  DeleteFiber(state.fiber_b);
  EXPECT_THAT(GetSwitchCallTexts(),
              ElementsAre("suspend A 1", "suspend B 2", "resume A 1"));
}

TEST_F(FiberSwitchCallbacksTest, SettingAnotherSetDies) {
  CHECK_FIBER_SUPPORT();
  static constexpr FiberSwitchCallbacks kIgnoreSwitches = {
      .on_suspend = IgnoreSuspend,
      .on_resume = IgnoreResume,
  };
  EXPECT_DEATH(SetFiberSwitchCallbacks(&kIgnoreSwitches),
               "one set of fiber switch callbacks");
}

TEST_F(FiberSwitchCallbacksTest, MissingCallbackDies) {
  CHECK_FIBER_SUPPORT();
  SetFiberSwitchCallbacks(nullptr);
  static constexpr FiberSwitchCallbacks kNoResume = {
      .on_suspend = IgnoreSuspend,
  };
  EXPECT_DEATH(SetFiberSwitchCallbacks(&kNoResume), "both");
}

TEST_F(FiberTest, FiberName) {
  CHECK_FIBER_SUPPORT();
  Fiber fiber = CreateFiber({}, 0, nullptr, +[](void* user_data) {});
  ASSERT_NE(fiber, nullptr);
  SetFiberName(fiber, "Test");
  EXPECT_EQ(GetFiberName(fiber), "Test");
  DeleteFiber(fiber);
}

TEST_F(FiberTest, LongFiberNameIsTrimmed) {
  CHECK_FIBER_SUPPORT();
  Fiber fiber = CreateFiber({}, 0, nullptr, +[](void* user_data) {});
  ASSERT_NE(fiber, nullptr);
  SetFiberName(fiber, std::string(200, 'a'));
  EXPECT_EQ(GetFiberName(fiber), std::string(127, 'a'));
  DeleteFiber(fiber);
}

TEST_F(FiberTest, FiberNameIsOnlyTheGivenText) {
  CHECK_FIBER_SUPPORT();
  Fiber fiber = CreateFiber({}, 0, nullptr, +[](void* user_data) {});
  ASSERT_NE(fiber, nullptr);
  const std::string text = "Test" + std::string(200, 'b');
  SetFiberName(fiber, std::string_view(text).substr(0, 4));
  EXPECT_EQ(GetFiberName(fiber), std::string_view("Test"));
  DeleteFiber(fiber);
}

TEST_F(FiberTest, AccessNulLFiberName) {
  CHECK_FIBER_SUPPORT();
  SetFiberName(nullptr, "Test");
  EXPECT_EQ(GetFiberName(nullptr), "null");
}

TEST_F(FiberTest, FiberData) {
  CHECK_FIBER_SUPPORT();
  Fiber fiber = CreateFiber({}, 0, nullptr, +[](void* user_data) {});
  ASSERT_NE(fiber, nullptr);
  auto data = std::make_unique<int>();
  SetFiberData(fiber, data.get());
  EXPECT_EQ(GetFiberData(fiber), data.get());
  auto new_data = std::make_unique<int>();
  EXPECT_EQ(SwapFiberData(fiber, new_data.get()), data.get());
  EXPECT_EQ(GetFiberData(fiber), new_data.get());
  DeleteFiber(fiber);
}

TEST_F(FiberTest, AccessNulLFiberData) {
  CHECK_FIBER_SUPPORT();
  auto data = std::make_unique<int>();
  SetFiberData(nullptr, data.get());
  EXPECT_EQ(GetFiberData(nullptr), nullptr);
  EXPECT_EQ(SwapFiberData(nullptr, data.get()), nullptr);
  EXPECT_EQ(GetFiberData(nullptr), nullptr);
}

TEST_F(FiberTest, IsFiberRunningNotStarted) {
  CHECK_FIBER_SUPPORT();
  Fiber fiber = CreateFiber(
      FiberOption::kSetThreadName, 4096, nullptr, +[](void* user_data) {});
  ASSERT_NE(fiber, nullptr);
  EXPECT_EQ(GetFiberState(fiber), FiberState::kSuspended);
  DeleteFiber(fiber);
}

TEST_F(FiberTest, IsFiberRunningWithinFiber) {
  CHECK_FIBER_SUPPORT();
  auto fiber_threads = CreateFiberThreads(
      std::vector<PlacedThread>(1), FiberOption::kSetThreadName, 4096, nullptr,
      +[](void* user_data) {
        EXPECT_EQ(GetFiberState(GetThisFiber()), FiberState::kRunning);
      });
  ASSERT_EQ(fiber_threads.size(), 1);
  WaitAndDeleteFibers(fiber_threads);
}

TEST_F(FiberTest, IsFiberRunningAfterSwitch) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;
    Fiber next_fiber = nullptr;
    absl::Notification running;
    absl::Notification wait;
  } state;
  state.next_fiber = CreateFiber(
      FiberOption::kSetThreadName, 4096, &state, +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        EXPECT_EQ(GetFiberState(GetThisFiber()), FiberState::kRunning);
        state.running.Notify();
        state.wait.WaitForNotification();
      });
  auto fiber_threads = CreateFiberThreads(
      std::vector<PlacedThread>(1), FiberOption::kSetThreadName, 4096, &state,
      +[](void* user_data) {
        auto& state = *static_cast<State*>(user_data);
        SwitchToFiber(state.next_fiber);
      });
  ASSERT_EQ(fiber_threads.size(), 1);
  state.running.WaitForNotification();
  EXPECT_EQ(GetFiberState(fiber_threads[0].fiber), FiberState::kSuspended);
  EXPECT_EQ(GetFiberState(state.next_fiber), FiberState::kRunning);
  state.wait.Notify();
  WaitAndDeleteFibers(fiber_threads);
  DeleteFiber(state.next_fiber);
}

TEST_F(FiberTest, ThreadAbuse) {
  CHECK_FIBER_SUPPORT();
  struct State {
    State() = default;

    FiberMain callback = nullptr;
    std::atomic<int> counter = 0;

    absl::Mutex mutex;
    Queue<Fiber> idle_fibers ABSL_GUARDED_BY(mutex){100};
    Queue<Fiber> fibers_to_idle ABSL_GUARDED_BY(mutex){100};
    std::vector<Fiber> all_fibers ABSL_GUARDED_BY(mutex);
  } state;

  state.callback = +[](void* user_data) {
    auto& state = *static_cast<State*>(user_data);

    bool done = false;
    while (!done) {
      int count = ++state.counter;
      if (count > 200) {
        done = true;
      }
      if (count % 50 == 0) {
        Fiber fiber = CreateFiber(FiberOption::kSetThreadName, 4096, &state,
                                  state.callback);
        EXPECT_NE(fiber, nullptr);
        state.mutex.Lock();
        state.all_fibers.push_back(fiber);
        state.idle_fibers.push(fiber);
        state.mutex.Unlock();
      }
      while (true) {
        state.mutex.Lock();
        if (state.fibers_to_idle.empty()) {
          break;
        }
        Fiber maybe_idle = state.fibers_to_idle.front();
        FiberState fiber_state = GetFiberState(maybe_idle);
        if (fiber_state != FiberState::kRunning) {
          state.fibers_to_idle.pop();
          if (fiber_state == FiberState::kSuspended) {
            state.idle_fibers.push(maybe_idle);
            break;
          }
        }
        state.mutex.Unlock();
      }
      Fiber next_fiber = nullptr;
      if (!done) {
        state.mutex.Await(absl::Condition(
            +[](State* state) ABSL_NO_THREAD_SAFETY_ANALYSIS {
              return !state->idle_fibers.empty();
            },
            &state));
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

  // A thread pinned to each hardware thread, or four unpinned threads if there
  // are fewer.
  const int num_threads = std::max(4, GetMaxConcurrency());
  const ThreadPlacement placement = PlaceThreads({.thread_count = num_threads});

  state.mutex.Lock();
  auto fiber_threads =
      CreateFiberThreads(placement.threads, FiberOption::kSetThreadName, 4096,
                         &state, state.callback);
  for (const auto& fiber_thread : fiber_threads) {
    state.all_fibers.push_back(fiber_thread.fiber);
  }
  for (int i = 0; i < 5; ++i) {
    Fiber fiber =
        CreateFiber(FiberOption::kSetThreadName, 4096, &state, state.callback);
    state.all_fibers.push_back(fiber);
    state.idle_fibers.push(fiber);
  }
  state.mutex.Unlock();

  while (GetRunningFiberCount() > 0) {
    std::this_thread::yield();
  }

  state.mutex.Lock();
  EXPECT_EQ(state.counter, 200 + num_threads);
  for (const auto& fiber_thread : fiber_threads) {
    JoinThread(fiber_thread.thread);
  }
  for (auto fiber : state.all_fibers) {
    DeleteFiber(fiber);
  }
  state.mutex.Unlock();
}

// Switches between fibers on many threads as fast as possible, so a fiber is
// often switched to just as another thread switches away from it.
TEST(FiberStressTest, SwitchFibersAcrossThreads) {
  CHECK_FIBER_SUPPORT();
  constexpr int kThreadCount = 16;
  constexpr int kSwitchesPerFiber = 5000;

  // Abseil's deadlock detection, which debug builds run on every lock, makes
  // this many switches far too slow.
  absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kIgnore);
  absl::Cleanup restore_deadlock_detection = [] {
    absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kAbort);
  };

  struct State {
    State() = default;
    std::atomic<bool> start = false;
    std::vector<Fiber> fibers;
    std::atomic<int> switch_count = 0;
  } state;

  // Each fiber switches to the next fiber that isn't running, until it has
  // switched enough times or there are no fibers to switch to.
  FiberMain fiber_main = +[](void* user_data) {
    auto* state = static_cast<State*>(user_data);

    // Spin rather than wait, so every thread starts switching at once.
    while (!state->start) {
      std::this_thread::yield();
    }
    const Fiber this_fiber = GetThisFiber();
    const int fiber_count = static_cast<int>(state->fibers.size());
    for (int i = 0; i < kSwitchesPerFiber; ++i) {
      bool switched = false;
      for (int j = 0; j < fiber_count; ++j) {
        const Fiber fiber = state->fibers[(i + j) % fiber_count];
        if (fiber != this_fiber && SwitchToFiber(fiber)) {
          switched = true;
          break;
        }
      }
      if (!switched) {
        return;
      }
      ASSERT_EQ(GetThisFiber(), this_fiber);
      ++state->switch_count;
    }
  };

  for (int i = 0; i < kThreadCount; ++i) {
    Fiber fiber = CreateFiber({}, 0, &state, fiber_main);
    ASSERT_NE(fiber, nullptr);
    state.fibers.push_back(fiber);
  }
  auto fiber_threads = CreateFiberThreads(
      std::vector<PlacedThread>(kThreadCount), {}, 0, &state, fiber_main);
  ASSERT_EQ(fiber_threads.size(), kThreadCount);
  for (const auto& fiber_thread : fiber_threads) {
    state.fibers.push_back(fiber_thread.fiber);
  }
  state.start = true;

  for (const auto& fiber_thread : fiber_threads) {
    JoinThread(fiber_thread.thread);
  }
  for (Fiber fiber : state.fibers) {
    DeleteFiber(fiber);
  }
  EXPECT_GT(state.switch_count, 0);
  EXPECT_EQ(GetRunningFiberCount(), 0);
}

}  // namespace
}  // namespace gb
