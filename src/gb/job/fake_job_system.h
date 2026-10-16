// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_JOB_FAKE_JOB_SYSTEM_H_
#define GB_JOB_FAKE_JOB_SYSTEM_H_

#include <deque>
#include <memory>
#include <string>
#include <string_view>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gb/base/context.h"
#include "gb/job/job_counter.h"
#include "gb/job/job_system.h"
#include "gb/job/job_types.h"

namespace gb {

// A JobSystem for tests, which runs jobs only when the test says.
//
// Run() queues a job, and RunPendingJobs() runs the queued jobs. Jobs run one
// at a time, each on a job thread (one where JobSystem::Get() returns this job
// system), so a test chooses the order work finishes in. Job counters,
// contexts, and job data work as JobSystem describes.
//
// Wait() runs queued jobs until the counter is done. If no queued jobs are left
// and the counter still isn't done, the process ends with a fatal error rather
// than hanging. The jobs it runs run inside the Wait() call, so the waiting job
// can't continue until each of them ends: one that waits on work the waiting
// job does after its own Wait() fails, although JobSystem allows it.
//
// This class is thread-safe. Jobs only run one at a time if Run() and
// RunPendingJobs() are called from one thread, or from jobs.
class FakeJobSystem : public JobSystem {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Destroying the job system runs every queued job, and any jobs that they
  // run.
  ~FakeJobSystem() override;

  //----------------------------------------------------------------------------
  // Job control
  //----------------------------------------------------------------------------

  // Sets the names of jobs that run before Run() returns, as if a thread had
  // picked them up at once, so a test can have work finish while the code that
  // started it is part way through. This replaces any names set before. An
  // empty name matches every job run without a name.
  //
  // Each of these jobs runs on a new job thread of its own, while Run() waits
  // for it to finish. A job that needs a lock held by the code that ran it
  // deadlocks.
  void SetImmediateJobs(absl::Span<const std::string_view> names);

  // Runs the queued jobs, and the jobs they queue, until none are left.
  void RunPendingJobs();

 protected:
  bool DoRun(std::string_view name, JobCounter* counter, Context* context,
             absl::AnyInvocable<void()> callback) override;
  void DoWait(JobCounter* counter) override;
  Context& DoGetContext() override;
  JobData& DoGetJobData() override;

 private:
  struct Job {
    std::string name;
    absl::AnyInvocable<void()> callback;

    // Incremented when the job is run, and decremented when it ends, if set.
    JobCounter* counter = nullptr;

    // Created on demand.
    std::unique_ptr<Context> context;

    JobData data;
  };

  // Calls `callback` on a new job thread, and waits for it to return.
  void RunOnJobThread(absl::AnyInvocable<void()> callback);

  // Removes the next queued job, or returns null if there are none.
  std::unique_ptr<Job> PopPendingJob();

  // Runs queued jobs until none are left.
  void RunPendingJobsOnThisThread();

  // Runs the job on this thread, which must be a job thread, and ends it.
  void RunJob(std::unique_ptr<Job> job);

  // The job running on this thread, if it is a job thread.
  static thread_local Job* s_current_job;

  absl::Mutex mutex_;
  absl::flat_hash_set<std::string> immediate_jobs_ ABSL_GUARDED_BY(mutex_);
  std::deque<std::unique_ptr<Job>> pending_jobs_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace gb

#endif  // GB_JOB_FAKE_JOB_SYSTEM_H_
