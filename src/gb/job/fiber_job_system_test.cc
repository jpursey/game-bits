// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/fiber_job_system.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "gb/base/context_builder.h"
#include "gb/base/fake_clock.h"
#include "gb/profile/fake_profiler_cpu.h"
#include "gb/profile/profile_point.h"
#include "gb/profile/profile_timer.h"
#include "gb/profile/profiler.h"
#include "gb/test/log_recorder.h"
#include "gb/thread/fiber.h"
#include "gb/thread/thread.h"
#include "gb/thread/thread_placement.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Contains;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;
using ::testing::ValuesIn;

std::string_view GetPinningName(ThreadPinning pinning) {
  switch (pinning) {
    case ThreadPinning::kNone:
      return "kNone";
    case ThreadPinning::kHardwareThread:
      return "kHardwareThread";
    case ThreadPinning::kCore:
      return "kCore";
  }
  return "unknown";
}

std::vector<uint64_t> GetAffinities(const ThreadPlacement& placement) {
  std::vector<uint64_t> affinities;
  for (const PlacedThread& thread : placement.threads) {
    affinities.push_back(thread.affinity);
  }
  return affinities;
}

std::vector<int> GetCores(const ThreadPlacement& placement) {
  std::vector<int> cores;
  for (const PlacedThread& thread : placement.threads) {
    cores.push_back(thread.core);
  }
  return cores;
}

// Waits until every job thread is idle. A profiling job system's main Profiler
// then reads what each recorded.
void WaitUntilIdle(const FiberJobSystem& job_system) {
  while (job_system.GetIdleThreadCount() < job_system.GetThreadCount()) {
    std::this_thread::yield();
  }
}

struct TestParams {
  TestParams() = default;
  TestParams(int in_thread_count, ThreadPinning in_pinning,
             int in_reserved_cores = 0)
      : thread_count(in_thread_count),
        pinning(in_pinning),
        reserved_cores(in_reserved_cores) {}

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const TestParams& params) {
    absl::Format(&sink, "{thread_count=%d, pinning=%s, reserved_cores=%d}",
                 params.thread_count, GetPinningName(params.pinning),
                 params.reserved_cores);
  }

  int thread_count = 0;
  ThreadPinning pinning = ThreadPinning::kHardwareThread;
  int reserved_cores = 0;
};

class FiberJobSystemTest : public ::testing::TestWithParam<TestParams> {
 protected:
  void SetUp() override {
    if (!SupportsFibers()) {
      GTEST_SKIP() << "Fibers are not supported";
    }
    // SetFiberVerboseLogging(true);  // Uncomment for more detailed logs
    FiberJobSystem::SetVerboseLogging(true);
    job_system_ = CreateJobSystem();
    ASSERT_NE(job_system_, nullptr);
    ASSERT_EQ(job_system_->GetThreadCount(),
              GetExpectedPlacement().threads.size());
  }
  void TearDown() override {
    job_system_.reset();
    FiberJobSystem::SetVerboseLogging(false);
    SetFiberVerboseLogging(false);
  }

  std::unique_ptr<FiberJobSystem> CreateJobSystem() {
    return FiberJobSystem::Create(
        ContextBuilder()
            .SetValue<int>(FiberJobSystem::kKeyThreadCount,
                           GetParam().thread_count)
            .SetValue<ThreadPinning>(FiberJobSystem::kKeyThreadPinning,
                                     GetParam().pinning)
            .SetValue<int>(FiberJobSystem::kKeyReservedCores,
                           GetParam().reserved_cores)
            .SetValue<bool>(FiberJobSystem::kKeySetFiberNames, true)
            .Build());
  }

  // Returns where a job system made by CreateJobSystem() places its threads.
  ThreadPlacement GetExpectedPlacement() const {
    return PlaceThreads({.pinning = GetParam().pinning,
                         .reserved_cores = GetParam().reserved_cores,
                         .thread_count = GetParam().thread_count});
  }

  std::unique_ptr<FiberJobSystem> job_system_;
};

TEST_P(FiberJobSystemTest, MainThreadIsNotJob) {
  EXPECT_EQ(JobSystem::Get(), nullptr);
}

TEST_P(FiberJobSystemTest, PlacementIsFromPlaceThreads) {
  const ThreadPlacement expected = GetExpectedPlacement();
  const ThreadPlacement& placement = job_system_->GetPlacement();
  EXPECT_EQ(GetAffinities(placement), GetAffinities(expected));
  EXPECT_EQ(GetCores(placement), GetCores(expected));
  EXPECT_EQ(placement.reserved_cores, expected.reserved_cores);
}

TEST_P(FiberJobSystemTest, JobThreadsHaveTheirPlacedAffinities) {
  // Each job blocks its thread until every thread has a job, so each job runs
  // on a different job thread.
  const int thread_count = job_system_->GetThreadCount();
  absl::Mutex mutex;
  std::vector<uint64_t> affinities;
  absl::Notification all_started;
  for (int i = 0; i < thread_count; ++i) {
    EXPECT_TRUE(job_system_->Run([&] {
      {
        absl::MutexLock lock(&mutex);
        affinities.push_back(GetThreadAffinity(GetThisThread()));
        if (static_cast<int>(affinities.size()) == thread_count) {
          all_started.Notify();
        }
      }
      all_started.WaitForNotificationWithTimeout(absl::Seconds(10));
    }));
  }

  // Destroying the job system waits for the jobs, which use these locals.
  job_system_.reset();
  EXPECT_THAT(affinities,
              UnorderedElementsAreArray(GetAffinities(GetExpectedPlacement())));
}

TEST_P(FiberJobSystemTest, JobsOnAnIdleJobSystemRunOnTheFirstJobThread) {
  const ThreadPlacement& placement = job_system_->GetPlacement();
  if (placement.threads[0].affinity == 0) {
    GTEST_SKIP() << "Job threads are not pinned, so can't be told apart";
  }
  for (int i = 0; i < 10; ++i) {
    WaitUntilIdle(*job_system_);
    uint64_t affinity = 0;
    absl::Notification done;
    EXPECT_TRUE(job_system_->Run([&affinity, &done] {
      affinity = GetThreadAffinity(GetThisThread());
      done.Notify();
    }));
    done.WaitForNotification();
    EXPECT_EQ(affinity, placement.threads[0].affinity) << "Job " << i;
  }
}

TEST_P(FiberJobSystemTest, JobsRunOnTheFirstIdleJobThread) {
  const ThreadPlacement placement = job_system_->GetPlacement();
  if (placement.threads[0].affinity == 0) {
    GTEST_SKIP() << "Job threads are not pinned, so can't be told apart";
  }

  // Each job holds its job thread until the end, so the next job runs on the
  // next job thread.
  const int job_count = std::min(job_system_->GetThreadCount(), 4);
  std::vector<uint64_t> affinities(job_count, 0);
  std::atomic<int> started_count = 0;
  absl::Notification release;
  WaitUntilIdle(*job_system_);
  for (int i = 0; i < job_count; ++i) {
    EXPECT_TRUE(job_system_->Run([&affinities, &started_count, &release, i] {
      affinities[i] = GetThreadAffinity(GetThisThread());
      ++started_count;
      release.WaitForNotification();
    }));
    while (started_count < i + 1) {
      std::this_thread::yield();
    }
  }
  release.Notify();

  // Destroying the job system waits for the jobs, which use these locals.
  job_system_.reset();
  std::vector<uint64_t> expected = GetAffinities(placement);
  expected.resize(job_count);
  EXPECT_EQ(affinities, expected);
}

TEST_P(FiberJobSystemTest, RunJob) {
  std::atomic<int> call_count = 0;
  absl::Notification notify;
  EXPECT_TRUE(job_system_->Run([this, &call_count, &notify] {
    EXPECT_EQ(job_system_.get(), JobSystem::Get());
    ++call_count;
    notify.Notify();
  }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 1);
}

TEST_P(FiberJobSystemTest, RunJobWithName) {
  std::atomic<int> call_count = 0;
  absl::Notification notify;
  EXPECT_TRUE(job_system_->Run("TestJob", [this, &call_count, &notify] {
    EXPECT_EQ(job_system_.get(), JobSystem::Get());
    EXPECT_EQ(GetFiberName(GetThisFiber()), "TestJob");
    ++call_count;
    notify.Notify();
  }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 1);
}

TEST_P(FiberJobSystemTest, RunJobWithContext) {
  std::atomic<int> call_count = 0;
  absl::Notification notify;
  EXPECT_TRUE(job_system_->Run(
      ContextBuilder().SetValue<int>(42).Build(), [this, &call_count, &notify] {
        EXPECT_EQ(job_system_.get(), JobSystem::Get());
        EXPECT_EQ(JobSystem::GetContext().GetValue<int>(), 42);
        ++call_count;
        notify.Notify();
      }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 1);
}

TEST_P(FiberJobSystemTest, RunJobWithNameAndContext) {
  std::atomic<int> call_count = 0;
  absl::Notification notify;
  EXPECT_TRUE(
      job_system_->Run("TestJob", ContextBuilder().SetValue<int>(42).Build(),
                       [this, &call_count, &notify] {
                         EXPECT_EQ(job_system_.get(), JobSystem::Get());
                         EXPECT_EQ(JobSystem::GetContext().GetValue<int>(), 42);
                         EXPECT_EQ(GetFiberName(GetThisFiber()), "TestJob");
                         ++call_count;
                         notify.Notify();
                       }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 1);
}

TEST_P(FiberJobSystemTest, WaitOnJob) {
  JobCounter counter;
  std::atomic<int> call_count = 0;
  absl::Notification notify;
  EXPECT_TRUE(job_system_->Run(&counter, [&call_count] {
    absl::SleepFor(absl::Milliseconds(10));
    ++call_count;
  }));
  EXPECT_TRUE(job_system_->Run([&counter, &call_count, &notify] {
    JobSystem::Wait(&counter);
    EXPECT_EQ(call_count, 1);
    notify.Notify();
  }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 1);
}

TEST_P(FiberJobSystemTest, WaitOnCompletedJob) {
  JobCounter counter;
  std::atomic<int> call_count = 0;

  absl::Notification notify_1;
  EXPECT_TRUE(job_system_->Run(&counter, [&call_count, &notify_1] {
    ++call_count;
    notify_1.Notify();
  }));
  notify_1.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 1);

  absl::Notification notify_2;
  EXPECT_TRUE(job_system_->Run([&counter, &call_count, &notify_2] {
    JobSystem::Wait(&counter);
    EXPECT_EQ(call_count, 1);
    notify_2.Notify();
  }));
  notify_2.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 1);
}

TEST_P(FiberJobSystemTest, WaitOnMultipleJobs) {
  JobCounter counter;
  std::atomic<int> call_count = 0;
  absl::Notification notify;
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(job_system_->Run(&counter, [&call_count] {
      absl::SleepFor(absl::Milliseconds(10));
      ++call_count;
    }));
  }
  EXPECT_TRUE(job_system_->Run([&counter, &call_count, &notify] {
    JobSystem::Wait(&counter);
    EXPECT_EQ(call_count, 10);
    notify.Notify();
  }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 10);
}

TEST_P(FiberJobSystemTest, MultipleJobsWaitOnOneCounter) {
  JobCounter counter;
  std::atomic<int> call_count = 0;
  absl::Notification notify_start;
  absl::Notification notify_complete;
  EXPECT_TRUE(job_system_->Run(
      &counter, [&notify_start] { notify_start.WaitForNotification(); }));
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(job_system_->Run([&notify_complete, &counter, &call_count] {
      JobSystem::Wait(&counter);
      if (++call_count == 10) {
        notify_complete.Notify();
      }
    }));
  }
  notify_start.Notify();
  notify_complete.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 10);
}

TEST_P(FiberJobSystemTest, WaitResumesOnAnotherThread) {
  const int thread_count = job_system_->GetThreadCount();
  if (thread_count < 2) {
    GTEST_SKIP() << "A job can only resume on another thread with two threads";
  }

  // The waiting job starts on one thread, and the job it waits on runs on a
  // second. Every other thread, including the first, is then blocked, so the
  // waiting job can only resume on the second.
  JobCounter counter;
  absl::Notification counted_started;
  absl::Notification release_counted;
  std::thread::id counted_id;
  Thread counted_thread = nullptr;
  std::atomic<int> blocked_count = 0;
  absl::Notification all_blocked;
  absl::Notification release_blocked;
  absl::Notification done;
  EXPECT_TRUE(job_system_->Run([&] {
    EXPECT_TRUE(JobSystem::Get()->Run(&counter, [&] {
      counted_id = std::this_thread::get_id();
      counted_thread = GetThisThread();
      counted_started.Notify();
      release_counted.WaitForNotification();
    }));

    // This blocks the thread, rather than waiting the job, so the counted job
    // starts on another thread.
    counted_started.WaitForNotification();
    for (int i = 0; i < thread_count - 1; ++i) {
      EXPECT_TRUE(JobSystem::Get()->Run([&] {
        if (++blocked_count == thread_count - 1) {
          all_blocked.Notify();
        }
        release_blocked.WaitForNotification();
      }));
    }
    const Thread start_thread = GetThisThread();
    JobSystem::Wait(&counter);
    EXPECT_EQ(std::this_thread::get_id(), counted_id);
    EXPECT_NE(GetThisThread(), start_thread);
    EXPECT_EQ(GetThisThread(), counted_thread);
    EXPECT_EQ(JobSystem::Get(), job_system_.get());
    done.Notify();
  }));
  all_blocked.WaitForNotificationWithTimeout(absl::Seconds(10));
  release_counted.Notify();
  done.WaitForNotificationWithTimeout(absl::Seconds(10));
  release_blocked.Notify();

  // Destroying the job system waits for the jobs, which use these locals.
  job_system_.reset();
}

TEST_P(FiberJobSystemTest, ContextIsDestroyedAtJobEnd) {
  std::atomic<int> call_count = 0;
  absl::Notification notify;
  struct TestStruct {
    explicit TestStruct(absl::Notification* in_notify) : notify(in_notify) {}
    ~TestStruct() { notify->Notify(); }
    absl::Notification* notify;
  };
  EXPECT_TRUE(job_system_->Run(
      ContextBuilder().SetNew<TestStruct>(&notify).Build(), [&call_count] {
        absl::SleepFor(absl::Milliseconds(10));
        ++call_count;
      }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 1);
}

TEST_P(FiberJobSystemTest, SetData) {
  struct Counters {
    Counters() = default;
    std::atomic<int> create = 0;
    std::atomic<int> destroy = 0;
  };
  Counters counters;
  Context context;
  context.SetPtr(&counters);

  struct TestData {
    TestData() : counters(JobSystem::GetContext().GetPtr<Counters>()) {
      ++counters->create;
    }
    ~TestData() { ++counters->destroy; }
    Counters* const counters;
  };

  JobDataHandle handle_1 = job_system_->AllocDataHandle<TestData>();
  JobDataHandle handle_2 = job_system_->AllocDataHandle<TestData>();
  EXPECT_NE(handle_1, 0);
  EXPECT_NE(handle_2, 0);
  EXPECT_NE(handle_1, handle_2);

  JobCounter counter_1;
  JobCounter counter_2;
  std::atomic<int> call_count = 0;
  absl::Notification notify;
  EXPECT_TRUE(job_system_->Run(
      &counter_1, ContextBuilder().SetParent(&context).Build(),
      [&call_count, handle_1] {
        auto* counters = JobSystem::GetContext().GetPtr<Counters>();
        EXPECT_EQ(counters->create, 0);
        EXPECT_EQ(counters->destroy, 0);
        auto* test_data = JobSystem::GetData<TestData>(handle_1);
        EXPECT_NE(test_data, nullptr);
        EXPECT_EQ(test_data->counters, counters);
        EXPECT_EQ(counters->create, 1);
        EXPECT_EQ(counters->destroy, 0);
        absl::SleepFor(absl::Milliseconds(10));
        ++call_count;
      }));
  EXPECT_TRUE(job_system_->Run(
      &counter_2, ContextBuilder().SetParent(&context).Build(),
      [&counter_1, &call_count, handle_2] {
        JobSystem::Wait(&counter_1);
        auto* counters = JobSystem::GetContext().GetPtr<Counters>();
        EXPECT_EQ(counters->create, 1);
        EXPECT_EQ(counters->destroy, 1);
        auto* test_data = JobSystem::GetData<TestData>(handle_2);
        EXPECT_NE(test_data, nullptr);
        EXPECT_EQ(test_data->counters->create, 2);
        EXPECT_EQ(test_data->counters->destroy, 1);
        absl::SleepFor(absl::Milliseconds(10));
        ++call_count;
      }));
  EXPECT_TRUE(job_system_->Run([&counter_2, &call_count, &notify] {
    JobSystem::Wait(&counter_2);
    EXPECT_EQ(call_count, 2);
    notify.Notify();
  }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 2);
  EXPECT_EQ(counters.create, 2);
  EXPECT_EQ(counters.destroy, 2);
}

TEST_P(FiberJobSystemTest, SetDataWithCustomAlloc) {
  struct Counters {
    Counters() = default;
    std::atomic<int> create = 0;
    std::atomic<int> destroy = 0;
  };
  Counters counters;

  struct TestData {
    explicit TestData(Counters* in_counters) : counters(in_counters) {
      ++counters->create;
    }
    ~TestData() { ++counters->destroy; }
    Counters* const counters;
  };

  JobDataHandle handle = job_system_->AllocDataHandle<TestData>(
      [&counters]() { return new TestData(&counters); });
  EXPECT_NE(handle, 0);

  JobCounter counter;
  std::atomic<int> call_count = 0;
  absl::Notification notify;
  EXPECT_TRUE(job_system_->Run(&counter, [&counters, &call_count, handle] {
    EXPECT_EQ(counters.create, 0);
    EXPECT_EQ(counters.destroy, 0);
    auto* test_data = JobSystem::GetData<TestData>(handle);
    EXPECT_NE(test_data, nullptr);
    EXPECT_EQ(test_data->counters, &counters);
    EXPECT_EQ(counters.create, 1);
    EXPECT_EQ(counters.destroy, 0);
    absl::SleepFor(absl::Milliseconds(10));
    ++call_count;
  }));
  EXPECT_TRUE(job_system_->Run([&counter, &call_count, &notify] {
    JobSystem::Wait(&counter);
    notify.Notify();
  }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(counters.create, 1);
  EXPECT_EQ(counters.destroy, 1);
}

TEST_P(FiberJobSystemTest, JobHierarchy) {
  absl::Notification notify;
  std::atomic<uint32_t> count = 0;
  EXPECT_TRUE(job_system_->Run([&notify, &count] {
    JobCounter counter;
    auto* system = JobSystem::Get();
    for (uint32_t i = 0; i < 8; ++i) {
      EXPECT_TRUE(system->Run(&counter, [&count, i] {
        JobCounter sub_counter;
        std::atomic<uint32_t> sub_count = 0;
        auto* system = JobSystem::Get();
        for (uint32_t k = 0; k < 4; ++k) {
          EXPECT_TRUE(system->Run(&sub_counter, [&sub_count, k] {
            absl::SleepFor(absl::Milliseconds(k + 1));
            sub_count |= 1 << k;
          }));
        }
        system->Wait(&sub_counter);
        count |= sub_count.load() << (i * 4);
        absl::SleepFor(absl::Milliseconds(i + 1));
      }));
    }
    system->Wait(&counter);
    EXPECT_EQ(count, 0xFFFFFFFF);
    notify.Notify();
  }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(count, 0xFFFFFFFF);
}

TEST_P(FiberJobSystemTest, AllDataHandles) {
  const int max_handles = static_cast<int>(kMaxJobDataHandles);
  std::vector<JobDataHandle> handles;
  for (int i = 0; i < max_handles; ++i) {
    handles.push_back(job_system_->AllocDataHandle<int>());
    EXPECT_NE(handles.back(), kInvalidJobDataHandle);
  }
  EXPECT_EQ(job_system_->AllocDataHandle<int>(), kInvalidJobDataHandle);

  std::atomic<int> match_count = 0;
  EXPECT_TRUE(job_system_->Run([&handles, &match_count] {
    const int handle_count = static_cast<int>(handles.size());
    for (int i = 0; i < handle_count; ++i) {
      *JobSystem::GetData<int>(handles[i]) = i;
    }
    for (int i = 0; i < handle_count; ++i) {
      if (*JobSystem::GetData<int>(handles[i]) == i) {
        ++match_count;
      }
    }
  }));
  job_system_.reset();
  EXPECT_EQ(match_count, max_handles);
}

TEST_P(FiberJobSystemTest, WaitOnCompletedJobUsesNoFiber) {
  // A counter that no job was run with is already done.
  JobCounter counter;
  std::atomic<int> fiber_count = 0;
  FiberJobSystem* const job_system = job_system_.get();
  EXPECT_TRUE(job_system_->Run([job_system, &counter, &fiber_count] {
    JobSystem::Wait(&counter);
    fiber_count = job_system->GetFiberCount();
  }));
  const int thread_count = job_system_->GetThreadCount();
  job_system_.reset();
  EXPECT_EQ(fiber_count, thread_count);
}

TEST_P(FiberJobSystemTest, DestroyRunsPendingJobs) {
  std::atomic<int> call_count = 0;
  for (int i = 0; i < 1000; ++i) {
    EXPECT_TRUE(job_system_->Run([&call_count] { ++call_count; }));
  }
  job_system_.reset();
  EXPECT_EQ(call_count, 1000);
}

TEST_P(FiberJobSystemTest, DestroyResumesWaitingJobs) {
  std::atomic<int> call_count = 0;
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(job_system_->Run([&call_count] {
      JobCounter counter;
      for (int j = 0; j < 10; ++j) {
        EXPECT_TRUE(
            JobSystem::Get()->Run(&counter, [&call_count] { ++call_count; }));
      }
      JobSystem::Wait(&counter);
      ++call_count;
    }));
  }
  job_system_.reset();
  EXPECT_EQ(call_count, 110);
}

TEST_P(FiberJobSystemTest, VerboseLogging) {
  LogRecorder log_recorder;
  EXPECT_TRUE(job_system_->Run([] {}));
  job_system_.reset();
  const std::vector<LogRecorder::Message> messages =
      log_recorder.TakeMessages();
  if (GB_BUILD_ENABLE_JOB_LOGGING) {
    EXPECT_THAT(messages, Contains(Field(&LogRecorder::Message::text,
                                         HasSubstr("FiberJobSystem:"))));
  } else {
    EXPECT_THAT(messages, IsEmpty());
  }
}

TEST_P(FiberJobSystemTest, VerboseLoggingDisabled) {
  job_system_.reset();
  FiberJobSystem::SetVerboseLogging(false);
  LogRecorder log_recorder;
  job_system_ = CreateJobSystem();
  ASSERT_NE(job_system_, nullptr);
  EXPECT_TRUE(job_system_->Run([] {}));
  job_system_.reset();
  EXPECT_THAT(log_recorder.TakeMessages(),
              Not(Contains(Field(&LogRecorder::Message::text,
                                 HasSubstr("FiberJobSystem:")))));
}

TestParams test_params[] = {
    {1, ThreadPinning::kNone},
    {2, ThreadPinning::kHardwareThread},
    {0, ThreadPinning::kNone},
    {0, ThreadPinning::kHardwareThread},
    {0, ThreadPinning::kCore},
    {0, ThreadPinning::kNone, 1},
    {0, ThreadPinning::kHardwareThread, 1},
    {0, ThreadPinning::kCore, 1},
};
INSTANTIATE_TEST_SUITE_P(All, FiberJobSystemTest, ValuesIn(test_params));

TEST(FiberJobSystemCreateTest, ThreadsArePinnedToHardwareThreadsByDefault) {
  if (!SupportsFibers()) {
    GTEST_SKIP();
  }
  auto job_system = FiberJobSystem::Create(ContextBuilder().Build());
  ASSERT_NE(job_system, nullptr);
  const ThreadPlacement& placement = job_system->GetPlacement();
  const ThreadPlacement expected = PlaceThreads({});
  EXPECT_EQ(GetAffinities(placement), GetAffinities(expected));
  EXPECT_EQ(GetCores(placement), GetCores(expected));
}

TEST(FiberJobSystemCreateTest, NegativeThreadCountFails) {
  if (!SupportsFibers()) {
    GTEST_SKIP();
  }
  EXPECT_EQ(FiberJobSystem::Create(
                ContextBuilder()
                    .SetValue<int>(FiberJobSystem::kKeyThreadCount, -1)
                    .Build()),
            nullptr);
}

// With one job thread, the waited-on job runs on the waiting job's thread while
// it waits, in the same Profiler. Its run is the waiting scope's wait, which is
// only recorded if Wait()'s fiber switch reaches the profiler.
TEST(FiberJobSystemProfileTest, TimedScopeLeavesOutWaitedOnJob) {
  if (!SupportsFibers()) {
    GTEST_SKIP();
  }
  auto job_system = FiberJobSystem::Create(
      ContextBuilder()
          .SetValue<int>(FiberJobSystem::kKeyThreadCount, 1)
          .Build());
  ASSERT_NE(job_system, nullptr);

  // A Profiler is read on its own thread, so the job reads what it recorded.
  absl::Duration waiting_self_time;
  absl::Duration waiting_wait_time;
  absl::Duration other_self_time;
  absl::Duration other_wait_time;
  EXPECT_TRUE(job_system->Run([&] {
    FakeProfilerCpu ticks;
    Profiler profiler({.fake_cpu = &ticks});
    {
      ProfileScope<"JobWaitTest/Waiting"> scope;
      ticks.Advance(1);
      JobCounter counter;
      EXPECT_TRUE(JobSystem::Get()->Run(&counter, [&ticks] {
        ProfileScope<"JobWaitTest/Other"> scope;
        ticks.Advance(10);
      }));
      JobSystem::Wait(&counter);
      ticks.Advance(2);
    }
    waiting_self_time = profiler.GetSelfTime("JobWaitTest/Waiting");
    waiting_wait_time = profiler.GetWaitTime("JobWaitTest/Waiting");
    other_self_time = profiler.GetSelfTime("JobWaitTest/Other");
    other_wait_time = profiler.GetWaitTime("JobWaitTest/Other");
  }));

  // Destroying the job system waits for the job, which uses these locals.
  job_system.reset();
  EXPECT_EQ(waiting_self_time, absl::Nanoseconds(3));
  EXPECT_EQ(waiting_wait_time, absl::Nanoseconds(10));
  EXPECT_EQ(other_self_time, absl::Nanoseconds(10));
  EXPECT_EQ(other_wait_time, absl::ZeroDuration());
}

// Job systems whose Profilers all read `cpu_`. Each test declares the locals
// its jobs use before its job system, which is then destroyed first.
class ProfiledFiberJobSystemTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!SupportsFibers()) {
      GTEST_SKIP() << "Fibers are not supported";
    }
  }

  std::unique_ptr<FiberJobSystem> CreateJobSystem(int thread_count,
                                                  bool profile_jobs = true) {
    return FiberJobSystem::Create(
        ContextBuilder()
            .SetValue<int>(FiberJobSystem::kKeyThreadCount, thread_count)
            .SetValue<ThreadPinning>(FiberJobSystem::kKeyThreadPinning,
                                     ThreadPinning::kNone)
            .SetValue<bool>(FiberJobSystem::kKeyProfileJobs, profile_jobs)
            .SetValue<Profiler::Options>(FiberJobSystem::kKeyProfilerOptions,
                                         Profiler::Options{.fake_cpu = &cpu_})
            .Build());
  }

  FakeProfilerCpu cpu_;
};

TEST_F(ProfiledFiberJobSystemTest, JobHasRunTimeAndStartDelay) {
  absl::Notification release;
  absl::Notification done;
  auto job_system = CreateJobSystem(1);
  ASSERT_NE(job_system, nullptr);

  // The job is queued behind one that blocks the only job thread until the
  // ticks have moved on.
  EXPECT_TRUE(job_system->Run("ProfiledJobTest/Blocker",
                              [&release] { release.WaitForNotification(); }));
  EXPECT_TRUE(job_system->Run("ProfiledJobTest/Job", [this, &done] {
    cpu_.Advance(30);
    done.Notify();
  }));
  cpu_.Advance(100);
  release.Notify();
  done.WaitForNotification();
  WaitUntilIdle(*job_system);

  const Profiler::JobSummary summary =
      job_system->GetProfiler()->GetJobSummary("ProfiledJobTest/Job");
  EXPECT_EQ(summary.runs, 1);
  EXPECT_EQ(summary.total, absl::Nanoseconds(30));
  EXPECT_EQ(summary.start_delay, absl::Nanoseconds(100));
  EXPECT_EQ(summary.resumes, 0);
}

TEST_F(ProfiledFiberJobSystemTest, WaitingJobsHaveResumeDelays) {
  absl::Notification release;
  JobCounter counter;
  std::atomic<int> resumed_count = 0;
  absl::Notification done;
  auto job_system = CreateJobSystem(1);
  ASSERT_NE(job_system, nullptr);

  // Both waiters wait on the counted job, which is queued after them, and are
  // ready to resume as it ends. The only job thread runs one, and then the
  // other, which resumes once the first has moved the ticks on.
  EXPECT_TRUE(job_system->Run("ProfiledJobTest/Blocker",
                              [&release] { release.WaitForNotification(); }));
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(job_system->Run("ProfiledJobTest/Waiter", [&, this] {
      JobSystem::Wait(&counter);
      cpu_.Advance(50);
      if (++resumed_count == 2) {
        done.Notify();
      }
    }));
  }
  EXPECT_TRUE(job_system->Run("ProfiledJobTest/Counted", &counter, [] {}));
  release.Notify();
  done.WaitForNotification();
  WaitUntilIdle(*job_system);

  const Profiler::JobSummary summary =
      job_system->GetProfiler()->GetJobSummary("ProfiledJobTest/Waiter");
  EXPECT_EQ(summary.runs, 2);
  EXPECT_EQ(summary.total, absl::Nanoseconds(100));
  EXPECT_EQ(summary.resumes, 2);
  EXPECT_EQ(summary.resume_delay, absl::Nanoseconds(25));
  EXPECT_EQ(summary.max_resume_delay, absl::Nanoseconds(50));
}

TEST_F(ProfiledFiberJobSystemTest, IdleJobThreadHasIdleTime) {
  absl::Notification done;
  auto job_system = CreateJobSystem(2);
  ASSERT_NE(job_system, nullptr);

  // With both job threads idle, the job runs on the first, so the second is
  // idle for the whole frame.
  WaitUntilIdle(*job_system);
  EXPECT_TRUE(job_system->Run("ProfiledJobTest/Frame job", [this, &done] {
    {
      ProfileFrame<"ProfiledJobTest/Frame"> frame;
      cpu_.Advance(100);
    }
    done.Notify();
  }));
  done.WaitForNotification();
  WaitUntilIdle(*job_system);

  const std::string report = job_system->GetProfiler()->GetReport();
  EXPECT_THAT(report, HasSubstr(R"(
Thread        Busy/frame    Max  Share  class 0  Idle/frame  Idle share
job thread 1       100ns  100ns   100%     100%           0       0.00%
job thread 2           0      0  0.00%        -       100ns        100%
)"));
}

TEST_F(ProfiledFiberJobSystemTest, FrameWaitsOnJobAndEndsOnAnotherThread) {
  JobCounter counter;
  absl::Notification counted_started;
  absl::Notification release_counted;
  absl::Notification blocked;
  absl::Notification release_blocked;
  absl::Notification done;
  Thread start_thread = nullptr;
  Thread end_thread = nullptr;
  auto job_system = CreateJobSystem(2);
  ASSERT_NE(job_system, nullptr);

  // The frame's job runs a counted job, which starts on the other job thread,
  // and then a blocker, which takes the frame's thread once the frame waits.
  // So the frame resumes on the counted job's thread, once it ends.
  EXPECT_TRUE(job_system->Run("ProfiledJobTest/Frame job", [&, this] {
    {
      ProfileFrame<"ProfiledJobTest/Frame"> frame;
      start_thread = GetThisThread();
      cpu_.Advance(10);
      EXPECT_TRUE(
          JobSystem::Get()->Run("ProfiledJobTest/Counted", &counter, [&] {
            counted_started.Notify();
            release_counted.WaitForNotification();
          }));

      // This blocks the thread, rather than waiting the job, so the counted
      // job starts on the other thread.
      counted_started.WaitForNotification();
      EXPECT_TRUE(JobSystem::Get()->Run("ProfiledJobTest/Blocker", [&] {
        blocked.Notify();
        release_blocked.WaitForNotification();
      }));
      JobSystem::Wait(&counter);
      end_thread = GetThisThread();
      cpu_.Advance(5);
    }
    done.Notify();
  }));
  blocked.WaitForNotification();
  cpu_.Advance(20);
  release_counted.Notify();
  done.WaitForNotification();
  release_blocked.Notify();
  WaitUntilIdle(*job_system);

  // The frame's time includes the time it waited.
  EXPECT_NE(end_thread, start_thread);
  const Profiler::FrameSummary summary =
      job_system->GetProfiler()->GetFrameSummary();
  EXPECT_EQ(summary.frames, 1);
  EXPECT_EQ(summary.total, absl::Nanoseconds(35));
}

TEST_F(ProfiledFiberJobSystemTest, EachNameIsItsOwnPoint) {
  std::atomic<int> run_count = 0;
  absl::Notification done;
  auto job_system = CreateJobSystem(1);
  ASSERT_NE(job_system, nullptr);
  auto callback = [&] {
    if (++run_count == 4) {
      done.Notify();
    }
  };
  EXPECT_TRUE(job_system->Run("ProfiledJobTest/A", callback));
  EXPECT_TRUE(job_system->Run("ProfiledJobTest/B", callback));
  EXPECT_TRUE(job_system->Run("ProfiledJobTest/A", callback));
  EXPECT_TRUE(job_system->Run(callback));
  done.WaitForNotification();
  WaitUntilIdle(*job_system);

  const Profiler* const profiler = job_system->GetProfiler();
  EXPECT_EQ(profiler->GetJobSummary("ProfiledJobTest/A").runs, 2);
  EXPECT_EQ(profiler->GetJobSummary("ProfiledJobTest/B").runs, 1);
  EXPECT_EQ(profiler->GetJobSummary("unnamed job").runs, 1);
}

TEST_F(ProfiledFiberJobSystemTest, NothingIsRecordedWithoutProfileJobs) {
  absl::Notification done;
  auto job_system = CreateJobSystem(1, /*profile_jobs=*/false);
  ASSERT_NE(job_system, nullptr);
  EXPECT_EQ(job_system->GetProfiler(), nullptr);

  // No point is registered for the job.
  EXPECT_TRUE(job_system->Run("ProfiledJobTest/Unprofiled",
                              [&done] { done.Notify(); }));
  done.WaitForNotification();
  EXPECT_FALSE(ProfilePoint::Find("ProfiledJobTest/Unprofiled").has_value());
}

TEST_F(ProfiledFiberJobSystemTest, CreateFailsWithMainInProfilerOptions) {
  Profiler main_profiler({.fake_cpu = &cpu_});
  EXPECT_EQ(FiberJobSystem::Create(
                ContextBuilder()
                    .SetValue<bool>(FiberJobSystem::kKeyProfileJobs, true)
                    .SetValue<Profiler::Options>(
                        FiberJobSystem::kKeyProfilerOptions,
                        Profiler::Options{.main = &main_profiler})
                    .Build()),
            nullptr);
}

TEST(FiberJobSystemDeathTest, DestroyTimesOut) {
  if (!SupportsFibers()) {
    GTEST_SKIP();
  }
  EXPECT_DEATH(
      {
        FakeClock clock;
        absl::Notification never_notified;
        auto job_system = FiberJobSystem::Create(
            ContextBuilder()
                .SetPtr<Clock>(&clock)
                .SetValue<int>(FiberJobSystem::kKeyThreadCount, 1)
                .SetValue<bool>(FiberJobSystem::kKeySetFiberNames, true)
                .Build());
        job_system->Run("StuckJob", [&never_notified] {
          never_notified.WaitForNotification();
        });
        job_system.reset();
      },
      "StuckJob");
}

}  // namespace
}  // namespace gb
