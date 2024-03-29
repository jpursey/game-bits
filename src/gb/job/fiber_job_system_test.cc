// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/fiber_job_system.h"

#include "absl/synchronization/notification.h"
#include "gb/base/context_builder.h"
#include "gb/thread/thread.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ValuesIn;

#define CHECK_FIBER_SUPPORT() \
  if (!SupportsFibers()) {    \
    return;                   \
  }

struct TestParams {
  TestParams() = default;
  TestParams(int in_thread_count, bool in_pin_threads)
      : thread_count(in_thread_count), pin_threads(in_pin_threads) {}

  int thread_count = 0;
  bool pin_threads = false;
};

inline void PrintTo(const TestParams& value, std::ostream* out) {
  *out << "{thread_count=" << value.thread_count
       << ", pin_threads=" << (value.pin_threads ? "true" : "false") << "}";
}

class FiberJobSystemTest : public ::testing::TestWithParam<TestParams> {
 protected:
  void SetUp() override {
    CHECK_FIBER_SUPPORT();
    // SetFiberVerboseLogging(true);  // Uncomment for more detailed logs
    FiberJobSystem::SetVerboseLogging(true);
    job_system_ = FiberJobSystem::Create(
        ContextBuilder()
            .SetValue<int>(FiberJobSystem::kKeyThreadCount,
                           GetParam().thread_count)
            .SetValue<bool>(FiberJobSystem::kKeyPinThreads,
                            GetParam().pin_threads)
            .SetValue<bool>(FiberJobSystem::kKeySetFiberNames, true)
            .Build());
    ASSERT_NE(job_system_, nullptr);
    if (GetParam().thread_count > 0) {
      ASSERT_EQ(job_system_->GetThreadCount(), GetParam().thread_count);
    } else if (GetParam().thread_count == 0) {
      ASSERT_EQ(job_system_->GetThreadCount(), GetMaxConcurrency());
    } else {
      ASSERT_EQ(job_system_->GetThreadCount(),
                std::max(GetMaxConcurrency() + GetParam().thread_count, 1));
    }
  }
  void TearDown() override {
    job_system_.reset();
    FiberJobSystem::SetVerboseLogging(false);
    SetFiberVerboseLogging(false);
  }

  std::unique_ptr<FiberJobSystem> job_system_;
};

TEST_P(FiberJobSystemTest, MainThreadIsNotJob) {
  CHECK_FIBER_SUPPORT();
  EXPECT_EQ(JobSystem::Get(), nullptr);
}

TEST_P(FiberJobSystemTest, RunJob) {
  CHECK_FIBER_SUPPORT();
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
  CHECK_FIBER_SUPPORT();
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
  CHECK_FIBER_SUPPORT();
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
  CHECK_FIBER_SUPPORT();
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
  CHECK_FIBER_SUPPORT();
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
  CHECK_FIBER_SUPPORT();
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
  CHECK_FIBER_SUPPORT();
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
  CHECK_FIBER_SUPPORT();
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
  CHECK_FIBER_SUPPORT();
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
  CHECK_FIBER_SUPPORT();

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
  CHECK_FIBER_SUPPORT();

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
  CHECK_FIBER_SUPPORT();
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

TestParams test_params[] = {
    {1, false},
    {2, true},
    {0, false},
    {-1, true},
};
INSTANTIATE_TEST_SUITE_P(All, FiberJobSystemTest, ValuesIn(test_params));

}  // namespace
}  // namespace gb
