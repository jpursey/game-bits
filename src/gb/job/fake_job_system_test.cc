// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/fake_job_system.h"

#include <string>
#include <vector>

#include "gb/base/context_builder.h"
#include "gb/thread/thread.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(FakeJobSystemTest, RunQueuesJobUntilRunPendingJobs) {
  FakeJobSystem job_system;
  std::vector<std::string> events;
  EXPECT_TRUE(job_system.Run([&events] { events.push_back("job"); }));
  EXPECT_THAT(events, IsEmpty());
  job_system.RunPendingJobs();
  EXPECT_THAT(events, ElementsAre("job"));
}

TEST(FakeJobSystemTest, JobsRunOnJobThread) {
  FakeJobSystem job_system;
  EXPECT_EQ(JobSystem::Get(), nullptr);
  JobSystem* job_thread_system = nullptr;
  Thread job_thread = nullptr;
  EXPECT_TRUE(job_system.Run([&job_thread_system, &job_thread] {
    job_thread_system = JobSystem::Get();
    job_thread = GetThisThread();
  }));
  job_system.RunPendingJobs();
  EXPECT_EQ(job_thread_system, &job_system);
  EXPECT_NE(job_thread, nullptr);
  EXPECT_NE(job_thread, GetThisThread());
  EXPECT_EQ(JobSystem::Get(), nullptr);
}

TEST(FakeJobSystemTest, RunPendingJobsRunsJobsTheyQueue) {
  FakeJobSystem job_system;
  std::vector<std::string> events;
  EXPECT_TRUE(job_system.Run([&events] {
    events.push_back("first");
    EXPECT_TRUE(
        JobSystem::Get()->Run([&events] { events.push_back("queued"); }));
  }));
  EXPECT_TRUE(job_system.Run([&events] { events.push_back("second"); }));
  job_system.RunPendingJobs();
  EXPECT_THAT(events, ElementsAre("first", "second", "queued"));
}

TEST(FakeJobSystemTest, ImmediateJobRunsBeforeRunReturns) {
  FakeJobSystem job_system;
  job_system.SetImmediateJobs({"Now"});
  std::vector<std::string> events;
  JobSystem* job_thread_system = nullptr;
  EXPECT_TRUE(job_system.Run("Now", [&events, &job_thread_system] {
    events.push_back("now");
    job_thread_system = JobSystem::Get();
  }));
  EXPECT_TRUE(
      job_system.Run("Later", [&events] { events.push_back("later"); }));
  EXPECT_THAT(events, ElementsAre("now"));
  EXPECT_EQ(job_thread_system, &job_system);
  job_system.RunPendingJobs();
  EXPECT_THAT(events, ElementsAre("now", "later"));
}

TEST(FakeJobSystemTest, ImmediateJobFinishesPartWayThroughCaller) {
  FakeJobSystem job_system;
  job_system.SetImmediateJobs({"Now"});
  std::vector<std::string> events;
  Thread caller_thread = nullptr;
  Thread immediate_thread = nullptr;
  EXPECT_TRUE(job_system.Run([&events, &caller_thread, &immediate_thread] {
    caller_thread = GetThisThread();
    events.push_back("caller start");
    EXPECT_TRUE(JobSystem::Get()->Run("Now", [&events, &immediate_thread] {
      immediate_thread = GetThisThread();
      events.push_back("now");
    }));
    events.push_back("caller end");
  }));
  job_system.RunPendingJobs();
  EXPECT_THAT(events, ElementsAre("caller start", "now", "caller end"));
  EXPECT_NE(immediate_thread, nullptr);
  EXPECT_NE(immediate_thread, caller_thread);
}

TEST(FakeJobSystemTest, SetImmediateJobsReplacesNames) {
  FakeJobSystem job_system;
  job_system.SetImmediateJobs({"A"});
  job_system.SetImmediateJobs({"B"});
  std::vector<std::string> events;
  EXPECT_TRUE(job_system.Run("A", [&events] { events.push_back("a"); }));
  EXPECT_TRUE(job_system.Run("B", [&events] { events.push_back("b"); }));
  EXPECT_THAT(events, ElementsAre("b"));
  job_system.SetImmediateJobs({});
  EXPECT_TRUE(job_system.Run("B", [&events] { events.push_back("b"); }));
  job_system.RunPendingJobs();
  EXPECT_THAT(events, ElementsAre("b", "a", "b"));
}

TEST(FakeJobSystemTest, WaitRunsQueuedJobsUntilCounterIsDone) {
  FakeJobSystem job_system;
  JobCounter counter;
  std::vector<std::string> events;
  EXPECT_TRUE(job_system.Run([&counter, &events] {
    events.push_back("wait start");
    JobSystem::Wait(&counter);
    events.push_back("wait end");
  }));
  EXPECT_TRUE(
      job_system.Run(&counter, [&events] { events.push_back("counted 1"); }));
  EXPECT_TRUE(
      job_system.Run(&counter, [&events] { events.push_back("counted 2"); }));
  EXPECT_TRUE(job_system.Run([&events] { events.push_back("after"); }));
  job_system.RunPendingJobs();
  EXPECT_THAT(events, ElementsAre("wait start", "counted 1", "counted 2",
                                  "wait end", "after"));
}

TEST(FakeJobSystemTest, WaitOnDoneCounterReturnsAtOnce) {
  FakeJobSystem job_system;
  JobCounter counter;
  std::vector<std::string> events;
  EXPECT_TRUE(
      job_system.Run(&counter, [&events] { events.push_back("counted"); }));
  job_system.RunPendingJobs();
  EXPECT_TRUE(job_system.Run([&counter, &events] {
    JobSystem::Wait(&counter);
    events.push_back("waited");
  }));
  EXPECT_TRUE(job_system.Run([&events] { events.push_back("after"); }));
  job_system.RunPendingJobs();
  EXPECT_THAT(events, ElementsAre("counted", "waited", "after"));
}

TEST(FakeJobSystemTest, ImmediateJobCanWait) {
  FakeJobSystem job_system;
  job_system.SetImmediateJobs({"Now"});
  JobCounter counter;
  std::vector<std::string> events;
  EXPECT_TRUE(
      job_system.Run(&counter, [&events] { events.push_back("counted"); }));
  EXPECT_TRUE(job_system.Run("Now", [&counter, &events] {
    JobSystem::Wait(&counter);
    events.push_back("waited");
  }));
  EXPECT_THAT(events, ElementsAre("counted", "waited"));
}

TEST(FakeJobSystemDeathTest, WaitWithNoQueuedJobsLeftFails) {
  EXPECT_DEATH(
      {
        FakeJobSystem job_system;
        JobCounter counter;
        job_system.Run("StuckJob", &counter,
                       [&counter] { JobSystem::Wait(&counter); });
        job_system.RunPendingJobs();
      },
      "StuckJob");
}

TEST(FakeJobSystemTest, EachJobHasItsOwnContext) {
  FakeJobSystem job_system;
  int outer_value = 0;
  int inner_value = 0;
  bool inner_has_outer_value = true;
  JobCounter counter;
  EXPECT_TRUE(job_system.Run(
      ContextBuilder().SetValue<int>(1).Build(),
      [&counter, &outer_value, &inner_value, &inner_has_outer_value] {
        EXPECT_TRUE(JobSystem::Get()->Run(
            &counter, ContextBuilder().SetValue<int>(2).Build(),
            [&inner_value] {
              inner_value = JobSystem::GetContext().GetValue<int>();
            }));
        EXPECT_TRUE(JobSystem::Get()->Run(&counter, [&inner_has_outer_value] {
          inner_has_outer_value = JobSystem::GetContext().Exists<int>();
        }));
        JobSystem::Wait(&counter);
        outer_value = JobSystem::GetContext().GetValue<int>();
      }));
  job_system.RunPendingJobs();
  EXPECT_EQ(outer_value, 1);
  EXPECT_EQ(inner_value, 2);
  EXPECT_FALSE(inner_has_outer_value);
}

TEST(FakeJobSystemTest, ContextIsDestroyedAtJobEnd) {
  FakeJobSystem job_system;
  struct TestStruct {
    explicit TestStruct(bool* in_destroyed) : destroyed(in_destroyed) {}
    ~TestStruct() { *destroyed = true; }
    bool* const destroyed;
  };
  bool destroyed = false;
  bool destroyed_in_job = true;
  EXPECT_TRUE(job_system.Run(
      ContextBuilder().SetNew<TestStruct>(&destroyed).Build(),
      [&destroyed, &destroyed_in_job] { destroyed_in_job = destroyed; }));
  job_system.RunPendingJobs();
  EXPECT_FALSE(destroyed_in_job);
  EXPECT_TRUE(destroyed);
}

TEST(FakeJobSystemTest, EachJobHasItsOwnData) {
  FakeJobSystem job_system;
  struct Counts {
    int create = 0;
    int destroy = 0;
  };
  struct TestData {
    explicit TestData(Counts* in_counts) : counts(in_counts) {
      ++counts->create;
    }
    ~TestData() { ++counts->destroy; }
    Counts* const counts;
    int value = 0;
  };
  Counts counts;
  const JobDataHandle handle = job_system.AllocDataHandle<TestData>(
      [&counts] { return new TestData(&counts); });
  EXPECT_NE(handle, kInvalidJobDataHandle);

  JobCounter counter;
  int inner_value = -1;
  int outer_value = -1;
  int destroyed_before_wait_end = -1;
  EXPECT_TRUE(job_system.Run([handle, &counts, &counter, &inner_value,
                              &outer_value, &destroyed_before_wait_end] {
    JobSystem::GetData<TestData>(handle)->value = 1;
    EXPECT_TRUE(JobSystem::Get()->Run(&counter, [handle, &inner_value] {
      inner_value = JobSystem::GetData<TestData>(handle)->value;
    }));
    JobSystem::Wait(&counter);
    destroyed_before_wait_end = counts.destroy;
    outer_value = JobSystem::GetData<TestData>(handle)->value;
  }));
  job_system.RunPendingJobs();
  EXPECT_EQ(inner_value, 0);
  EXPECT_EQ(outer_value, 1);
  EXPECT_EQ(destroyed_before_wait_end, 1);
  EXPECT_EQ(counts.create, 2);
  EXPECT_EQ(counts.destroy, 2);
}

TEST(FakeJobSystemTest, DestroyRunsPendingJobs) {
  std::vector<std::string> events;
  {
    FakeJobSystem job_system;
    EXPECT_TRUE(job_system.Run([&events] {
      events.push_back("first");
      EXPECT_TRUE(
          JobSystem::Get()->Run([&events] { events.push_back("queued"); }));
    }));
  }
  EXPECT_THAT(events, ElementsAre("first", "queued"));
}

}  // namespace
}  // namespace gb
