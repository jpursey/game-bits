// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/fiber_job_system.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/synchronization/notification.h"
#include "gb/base/context_builder.h"
#include "gb/base/fake_clock.h"
#include "gb/test/log_recorder.h"
#include "gb/thread/thread.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Contains;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::ValuesIn;

struct TestParams {
  TestParams() = default;
  TestParams(int in_thread_count, bool in_pin_threads)
      : thread_count(in_thread_count), pin_threads(in_pin_threads) {}

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const TestParams& params) {
    absl::Format(&sink, "{thread_count=%d, pin_threads=%v}",
                 params.thread_count, params.pin_threads);
  }

  int thread_count = 0;
  bool pin_threads = false;
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
    int thread_count = GetParam().thread_count;
    if (thread_count <= 0) {
      thread_count = std::max(GetMaxConcurrency() + thread_count, 1);
    }
    ASSERT_EQ(job_system_->GetThreadCount(),
              std::min(thread_count, FiberJobSystem::kMaxThreadCount));
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
            .SetValue<bool>(FiberJobSystem::kKeyPinThreads,
                            GetParam().pin_threads)
            .SetValue<bool>(FiberJobSystem::kKeySetFiberNames, true)
            .Build());
  }

  std::unique_ptr<FiberJobSystem> job_system_;
};

TEST_P(FiberJobSystemTest, MainThreadIsNotJob) {
  EXPECT_EQ(JobSystem::Get(), nullptr);
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
    {1, false},
    {2, true},
    {0, false},
    {-1, true},
};
INSTANTIATE_TEST_SUITE_P(All, FiberJobSystemTest, ValuesIn(test_params));

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
