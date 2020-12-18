// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_JOB_JOB_SYSTEM_H_
#define GB_JOB_JOB_SYSTEM_H_

#include "gb/base/callback.h"
#include "gb/job/job_counter.h"
#include "gb/job/job_types.h"

namespace gb {

// The JobSystem class is a scheduler for a set of jobs which are run on one or
// more job threads.
//
// Jobs may be scheduled from any thread by calling the Run method, which will
// asynchronously execute the job on a job thread. Most other operations are
// only callable from within a job system thread. To determine the which job
// system a thread is associated with (if any), call JobSystem::Get().
//
// This class is thread-safe.
class JobSystem {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  JobSystem(const JobSystem&) = delete;
  JobSystem(JobSystem&&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;
  JobSystem& operator=(JobSystem&&) = delete;
  virtual ~JobSystem() = default;

  // Returns the JobSystem for the current thread, iff it is a job system
  // managed thread. If the thread is *not* a job thread, this returns nullptr.
  static JobSystem* Get();

  //----------------------------------------------------------------------------
  // Job execution
  //
  // These may called from any thread.
  //----------------------------------------------------------------------------

  // Runs a single job.
  //
  // If a counter is provided, it must outlive the job. Counters can only be
  // used within a single JobSystem.
  bool Run(JobCounter* counter, Callback<void()> callback);
  bool Run(Callback<void()> callback);

  //----------------------------------------------------------------------------
  // Job operations
  //
  // These may only be called within the context of a job callback.
  //----------------------------------------------------------------------------

  // Blocks this job, waiting until the JobCounter reaches zero.
  //
  // Only counters for jobs run in this job system can be waited on.
  static void Wait(JobCounter* counter);

 protected:
  JobSystem() = default;

  //----------------------------------------------------------------------------
  // Derived class interface
  //----------------------------------------------------------------------------

  // Sets the job system for the current thread. Derived class must call this
  // for each thread they run jobs on.
  void SetThreadState();

  virtual bool DoRun(JobCounter* counter, Callback<void()> callback) = 0;
  virtual void DoWait(JobCounter* counter) = 0;
};

inline bool JobSystem::Run(JobCounter* counter, Callback<void()> callback) {
  return DoRun(counter, std::move(callback));
}

inline bool JobSystem::Run(Callback<void()> callback) {
  return DoRun(nullptr, std::move(callback));
}

inline void JobSystem::Wait(JobCounter* counter) { Get()->DoWait(counter); }

}  // namespace gb

#endif  // GB_JOB_JOB_SYSTEM_H_
