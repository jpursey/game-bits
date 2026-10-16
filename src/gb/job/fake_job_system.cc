// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/fake_job_system.h"

#include <utility>

#include "absl/log/check.h"
#include "gb/thread/thread.h"

namespace gb {

thread_local FakeJobSystem::Job* FakeJobSystem::s_current_job = nullptr;

FakeJobSystem::~FakeJobSystem() { RunPendingJobs(); }

void FakeJobSystem::SetImmediateJobs(absl::Span<const std::string_view> names) {
  absl::MutexLock lock(&mutex_);
  immediate_jobs_.clear();
  immediate_jobs_.insert(names.begin(), names.end());
}

void FakeJobSystem::RunPendingJobs() {
  {
    absl::MutexLock lock(&mutex_);
    if (pending_jobs_.empty()) {
      return;
    }
  }
  RunOnJobThread([this] { RunPendingJobsOnThisThread(); });
}

void FakeJobSystem::RunOnJobThread(absl::AnyInvocable<void()> callback) {
  struct ThreadStart {
    FakeJobSystem* job_system;
    absl::AnyInvocable<void()>* callback;
  };
  ThreadStart start = {this, &callback};
  const Thread thread = CreateThread(
      0, 0, &start, +[](void* user_data) {
        auto* const start = static_cast<ThreadStart*>(user_data);
        start->job_system->SetThreadState();
        (*start->callback)();
      });
  CHECK(thread != nullptr) << "FakeJobSystem could not create a job thread";
  JoinThread(thread);
}

std::unique_ptr<FakeJobSystem::Job> FakeJobSystem::PopPendingJob() {
  absl::MutexLock lock(&mutex_);
  if (pending_jobs_.empty()) {
    return nullptr;
  }
  std::unique_ptr<Job> job = std::move(pending_jobs_.front());
  pending_jobs_.pop_front();
  return job;
}

void FakeJobSystem::RunPendingJobsOnThisThread() {
  while (std::unique_ptr<Job> job = PopPendingJob()) {
    RunJob(std::move(job));
  }
}

void FakeJobSystem::RunJob(std::unique_ptr<Job> job) {
  // A job that waits runs other jobs on this thread before it ends.
  Job* const prev_job = std::exchange(s_current_job, job.get());
  job->callback();

  // Context must always be destructed *after* job data.
  FreeJobData(job->data);
  job->context.reset();
  s_current_job = prev_job;

  if (job->counter != nullptr) {
    // Nothing waits on a counter in this job system, so there are no waiters
    // to resume.
    JobCounter::Waiters waiters;
    job->counter->Decrement({}, waiters);
  }
}

bool FakeJobSystem::DoRun(std::string_view name, JobCounter* counter,
                          Context* context,
                          absl::AnyInvocable<void()> callback) {
  auto job = std::make_unique<Job>();
  job->name.assign(name.data(), name.size());
  job->callback = std::move(callback);
  job->counter = counter;
  if (context != nullptr) {
    job->context = std::make_unique<Context>(std::move(*context));
  }
  if (counter != nullptr) {
    counter->Increment({});
  }
  {
    absl::MutexLock lock(&mutex_);
    if (!immediate_jobs_.contains(name)) {
      pending_jobs_.push_back(std::move(job));
      return true;
    }
  }
  RunOnJobThread(
      [this, job = std::move(job)]() mutable { RunJob(std::move(job)); });
  return true;
}

void FakeJobSystem::DoWait(JobCounter* counter) {
  if (counter == nullptr) {
    return;
  }
  while (!counter->IsDone({})) {
    std::unique_ptr<Job> job = PopPendingJob();
    CHECK(job != nullptr)
        << "FakeJobSystem job \"" << s_current_job->name
        << "\" waits on a counter that no queued job will finish";
    RunJob(std::move(job));
  }
}

Context& FakeJobSystem::DoGetContext() {
  DCHECK(s_current_job != nullptr);
  auto& context = s_current_job->context;
  if (context == nullptr) {
    context = std::make_unique<Context>();
  }
  return *context;
}

JobSystem::JobData& FakeJobSystem::DoGetJobData() {
  DCHECK(s_current_job != nullptr);
  return s_current_job->data;
}

}  // namespace gb
