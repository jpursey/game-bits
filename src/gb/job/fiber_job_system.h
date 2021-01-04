// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_JOB_FIBER_JOB_SYSTEM_H_
#define GB_JOB_FIBER_JOB_SYSTEM_H_

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "gb/alloc/pool_allocator.h"
#include "gb/base/context.h"
#include "gb/base/queue.h"
#include "gb/base/validated_context.h"
#include "gb/job/job_system.h"
#include "gb/thread/fiber.h"

namespace gb {

// This class implements the JobSystem in terms of user space fibers.
//
// This is implemented in terms of the fiber API. Calling code must not
// interfere with fibers managed by this class (aka calling SwitchToFiber or
// DeleteFiber from/to a job system managed fiber), as that will break
// internal management and a crash or hang is likely.
//
// This class is thread-safe.
class FiberJobSystem : public JobSystem {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // OPTIONAL: This determines the number of threads the job system will run
  // jobs on (max concurrency). The value is interpretted as follows:
  // - If set and positive, this is the number of threads created.
  // - If not set or is set to zero, the number of threads created is set to the
  //   number of available hardware threads. This does not implicitly pin job
  //   threads to hardware threads, see kConstraintPinThreads.
  // - If set and negative, the number of threads created is set to the
  //   number of available hardware threads less this value (minimum of 1).
  //   This does not implicitly pin job threads to hardware threads, see
  //   kConstraintPinThreads.
  // This will never result in more than kMaxThreadCount threads.
  static inline constexpr int kMaxThreadCount = 64;
  static inline constexpr char* kKeyThreadCount = "thread_count";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintThreadCount, kInOptional, int,
                                     kKeyThreadCount);

  // OPTIONAL: If set to true (the default value), job threads will
  // preferentially be pinned to corresponding hardware threads. If
  // the requested thread count is specified to be larger than the maximum
  // hardware concurrency, then this constraint is ignored and threads will not
  // be pinned.
  static inline constexpr char* kKeyPinThreads = "pin_threads";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintPinThreads, kInOptional,
                                             bool, kKeyPinThreads, true);

  // OPTIONAL: If set to true, thread and fiber names will be updated with the
  // name of the job they are running. If this is not set, then job names will
  // be propagated only in debug builds.
  static inline constexpr char* kKeySetFiberNames = "set_fiber_names";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintSetFiberNames, kInOptional,
                                     bool, kKeySetFiberNames);

  using CreateContract =
      gb::ContextContract<kConstraintThreadCount, kConstraintPinThreads,
                          kConstraintSetFiberNames>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  virtual ~FiberJobSystem();

  // Creates a new fiber-based job system.
  //
  // If the platform does not support fibers (SupportsFibers() returns
  // false), this will return null.
  static std::unique_ptr<FiberJobSystem> Create(CreateContract contract);

  //----------------------------------------------------------------------------
  // Debugging
  //----------------------------------------------------------------------------

  static void SetVerboseLogging(bool enabled);

  //----------------------------------------------------------------------------
  // Attributes
  //----------------------------------------------------------------------------

  int GetThreadCount() const;
  int GetFiberCount() const;

 protected:
  bool DoRun(std::string_view name, JobCounter* counter,
             Callback<void()> callback) override;
  void DoWait(JobCounter* counter) override;

 private:
  // Represents a job tracked by the system.
  struct Job {
    Job() = default;

    // Optional name for the job.
    std::string name;

    // Callback that is executed to perform this job.
    Callback<void()> callback;

    // Run counter which if not null is incremented when the job is initially
    // queued, and decremented when the job completes.
    JobCounter* run_counter = nullptr;
  };

  struct FiberState {
    FiberState(FiberJobSystem* in_system) : system(in_system) {}

    // Job system for this fiber.
    FiberJobSystem* const system;

    // Fiber this state is for.
    Fiber fiber = nullptr;

    // Job this fiber is currently running.
    Job* job = nullptr;
  };

  FiberJobSystem();
  bool Init(ValidatedContext context);

  void JobMain();
  bool HasNoFibersInUse() const ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  static std::string_view GetJobName(Job* job);

  bool set_fiber_names_ = false;

  mutable absl::Mutex mutex_;  // All state is guarded by this mutex.

  // True if the the fiber system is running (not being destructed).
  bool running_ ABSL_GUARDED_BY(mutex_) = true;

  // Total number of threads and fibers created. These only increases, as fibers
  // are recycled.
  int total_thread_count_ ABSL_GUARDED_BY(mutex_) = 0;
  int total_fiber_count_ ABSL_GUARDED_BY(mutex_) = 0;

  // Allocates used for job and fiber state.
  PoolAllocator job_allocator_ ABSL_GUARDED_BY(mutex_);
  PoolAllocator fiber_allocator_ ABSL_GUARDED_BY(mutex_);

  // Pending jobs waiting for a fiber to become available.
  Queue<Job*> pending_jobs_ ABSL_GUARDED_BY(mutex_);

  // Pending fibers that are waiting for a thread to become available.
  Queue<FiberState*> pending_fibers_ ABSL_GUARDED_BY(mutex_);

  // Fibers that are idling on a thread waiting to receive a job to
  // process.
  absl::flat_hash_set<FiberState*> idle_fibers_ ABSL_GUARDED_BY(mutex_);

  // Fibers that are actively running a job on a thread.
  absl::flat_hash_set<FiberState*> running_fibers_ ABSL_GUARDED_BY(mutex_);

  // Contains fibers waiting on a job counter.
  absl::flat_hash_map<JobCounter*, std::vector<FiberState*>> waiting_fibers_
      ABSL_GUARDED_BY(mutex_);

  // Fibers that were created but are not currently in use.
  Queue<Fiber> unused_fibers_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace gb

#endif  // GB_JOB_FIBER_JOB_SYSTEM_H_
