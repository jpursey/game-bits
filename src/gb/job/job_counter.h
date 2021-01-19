// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_JOB_JOB_COUNTER_H_
#define GB_JOB_JOB_COUNTER_H_

#include "absl/container/inlined_vector.h"
#include "absl/synchronization/mutex.h"
#include "gb/job/job_types.h"

namespace gb {

// A JobCounter is used to synchronize work between jobs in a JobSystem.
//
// To use a JobCounter, create an instance of it and pass it in with the
// callback to run one or more jobs. The counter is incremented for every job
// that is started using this counter and decremented when each job completes.
// One or more other jobs can then wait on the JobCounter. When the counter
// reaches zero, all waiting jobs are unblocked and will continue executing.
//
// A JobCounter must remain valid for as long as any running or waiting jobs
// depend on it. A single JobCounter cannot be used across multiple JobSystem
// instances.
class JobCounter {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  JobCounter() : counter_(0) {}
  JobCounter(const JobCounter&) = delete;
  JobCounter(JobCounter&&) = delete;
  JobCounter& operator=(const JobCounter&) = delete;
  JobCounter& operator=(JobCounter&&) = delete;
  ~JobCounter() = default;

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  typedef absl::InlinedVector<void*, 1> Waiters;

  // Increments the counter.
  void Increment(JobInternal) {
    absl::WriterMutexLock lock(&mutex_);
    ++counter_;
  }

  // Decrements the counter and returns any waiters that are now unblocked.
  //
  // Returns true if waiters were returned.
  bool Decrement(JobInternal, Waiters& waiters) {
    absl::WriterMutexLock lock(&mutex_);
    if (--counter_ > 0) {
      return false;
    }

    // absl::InlinedVector does not have a defined "moved-from" state, so we
    // cannot simply std::move waiters.
    waiters.swap(waiters_);
    waiters_.clear();
    return true;
  }

  // Adds a waiter if the counter is not already complete.
  //
  // Returns true if the waiter was added (otherwise the counter is already at
  // zero).
  bool AddWaiter(JobInternal, void* waiter) {
    absl::WriterMutexLock lock(&mutex_);
    if (counter_ == 0) {
      return false;
    }
    waiters_.push_back(waiter);
    return true;
  }

 private:
  mutable absl::Mutex mutex_;
  int counter_ ABSL_GUARDED_BY(mutex_);
  Waiters waiters_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace gb

#endif  // GB_JOB_JOB_COUNTER_H_
