// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/fiber_job_system.h"

#include "absl/synchronization/notification.h"
#include "gb/base/context_builder.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ValuesIn;

#define CHECK_FIBER_SUPPORT() \
  if (!SupportsJobFibers()) { \
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
    // SetJobFiberVerboseLogging(true);  // Uncomment for more detailed logs
    FiberJobSystem::SetVerboseLogging(true);
    job_system_ = FiberJobSystem::Create(
        ContextBuilder()
            .SetValue<int>(FiberJobSystem::kKeyThreadCount,
                           GetParam().thread_count)
            .SetValue<bool>(FiberJobSystem::kKeyPinThreads,
                            GetParam().pin_threads)
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
    SetJobFiberVerboseLogging(false);
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
    JobSystem::Get()->Wait(&counter);
    EXPECT_EQ(call_count, 1);
    notify.Notify();
  }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
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
    JobSystem::Get()->Wait(&counter);
    EXPECT_EQ(call_count, 10);
    notify.Notify();
  }));
  notify.WaitForNotificationWithTimeout(absl::Seconds(10));
}

TEST_P(FiberJobSystemTest, MultipleJobsWaitOnOneCounter) {
  CHECK_FIBER_SUPPORT();
  JobCounter counter;
  std::atomic<int> call_count = 0;
  absl::Notification notify_start;
  absl::Notification notify_complete;
  EXPECT_TRUE(job_system_->Run(&counter, [&notify_start, &call_count] {
    notify_start.WaitForNotification();
  }));
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(job_system_->Run([&notify_complete, &counter, &call_count] {
      JobSystem::Get()->Wait(&counter);
      if (++call_count == 10) {
        notify_complete.Notify();
      }
    }));
  }
  notify_start.Notify();
  notify_complete.WaitForNotificationWithTimeout(absl::Seconds(10));
}

TEST_P(FiberJobSystemTest, JobHierarchy) {
  CHECK_FIBER_SUPPORT();
  absl::Notification notify;
  EXPECT_TRUE(job_system_->Run([&notify] {
    JobCounter counter;
    std::atomic<uint32_t> count = 0;
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
