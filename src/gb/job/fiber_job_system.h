// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_JOB_FIBER_JOB_SYSTEM_H_
#define GB_JOB_FIBER_JOB_SYSTEM_H_

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "concurrentqueue.h"
#include "gb/alloc/pool_allocator.h"
#include "gb/base/clock.h"
#include "gb/base/context.h"
#include "gb/base/validated_context.h"
#include "gb/job/job_system.h"
#include "gb/job/job_types.h"
#include "gb/thread/thread_types.h"
#include "lightweightsemaphore.h"

namespace gb {

// This class implements the JobSystem in terms of user space fibers.
//
// This is implemented in terms of the fiber API. Calling code must not
// interfere with fibers managed by this class (aka calling SwitchToFiber or
// DeleteFiber from/to a job system managed fiber), as that will break
// internal management and a crash or hang is likely.
//
// Job threads that have no work spin briefly, and then sleep until a job is
// ready to run.
//
// This class is thread-safe.
class FiberJobSystem : public JobSystem {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // OPTIONAL: This determines the number of threads the job system will run
  // jobs on (max concurrency). The value is interpreted as follows:
  // - If set and positive, this is the number of threads created.
  // - If not set or is set to zero, the number of threads created is set to the
  //   number of available hardware threads. This does not implicitly pin job
  //   threads to hardware threads, see kConstraintPinThreads.
  // - If set and negative, the number of threads created is set to the
  //   number of available hardware threads reduced by its magnitude (minimum
  //   of 1). This does not implicitly pin job threads to hardware threads, see
  //   kConstraintPinThreads.
  // This will never result in more than kMaxThreadCount threads.
  static inline constexpr int kMaxThreadCount = 64;
  static inline constexpr const char* kKeyThreadCount = "thread_count";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintThreadCount, kInOptional, int,
                                     kKeyThreadCount);

  // OPTIONAL: If set to true (the default value), job threads will
  // preferentially be pinned to corresponding hardware threads. If
  // the requested thread count is specified to be larger than the maximum
  // hardware concurrency, then this constraint is ignored and threads will not
  // be pinned.
  static inline constexpr const char* kKeyPinThreads = "pin_threads";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintPinThreads, kInOptional,
                                             bool, kKeyPinThreads, true);

  // OPTIONAL: If set to true, thread and fiber names will be updated with the
  // name of the job they are running. If this is not set, then job names will
  // be propagated only in debug builds. Job names are only kept when they are
  // propagated.
  static inline constexpr const char* kKeySetFiberNames = "set_fiber_names";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintSetFiberNames, kInOptional,
                                     bool, kKeySetFiberNames);

  // OPTIONAL: The longest that destroying the job system waits for its jobs to
  // finish (10 seconds by default). If they haven't all finished by then, the
  // process is ended with a fatal error, which names them if job names are
  // kept (see kConstraintSetFiberNames). Set this to absl::InfiniteDuration()
  // to wait for as long as they take.
  static inline constexpr const char* kKeyShutdownTimeout = "shutdown_timeout";
  static inline constexpr absl::Duration kDefaultShutdownTimeout =
      absl::Seconds(10);
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintShutdownTimeout,
                                             kInOptional, absl::Duration,
                                             kKeyShutdownTimeout,
                                             kDefaultShutdownTimeout);

  // OPTIONAL: The clock used to time the shutdown timeout. If this is not set,
  // the realtime clock is used. It must outlive the job system.
  static GB_CONTEXT_CONSTRAINT(kConstraintClock, kInOptional, Clock);

  using CreateContract =
      ContextContract<kConstraintThreadCount, kConstraintPinThreads,
                      kConstraintSetFiberNames, kConstraintShutdownTimeout,
                      kConstraintClock>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Destroying the job system waits for every job to finish, including pending
  // jobs, and any jobs that they run. Jobs may not be run from other threads
  // once it is being destroyed.
  ~FiberJobSystem() override;

  // Creates a new fiber-based job system.
  //
  // If the platform does not support fibers (SupportsFibers() returns
  // false), this will return null.
  static std::unique_ptr<FiberJobSystem> Create(CreateContract contract);

  //----------------------------------------------------------------------------
  // Debugging
  //----------------------------------------------------------------------------

  // Sets whether all fiber job systems log what they do, at INFO level.
  //
  // This only has an effect if GB_BUILD_ENABLE_JOB_LOGGING is defined to be
  // non-zero (by default, this is true in debug builds).
  static void SetVerboseLogging(bool enabled);

  //----------------------------------------------------------------------------
  // Attributes
  //----------------------------------------------------------------------------

  int GetThreadCount() const;
  int GetFiberCount() const;

 protected:
  bool DoRun(std::string_view name, JobCounter* counter, Context* context,
             absl::AnyInvocable<void()> callback) override;
  void DoWait(JobCounter* counter) override;
  Context& DoGetContext() override;
  JobData& DoGetJobData() override;

 private:
  // Represents a job tracked by the system.
  struct Job {
    Job() = default;

    // Optional name for the job.
    std::string name;

    // Callback that is executed to perform this job.
    absl::AnyInvocable<void()> callback;

    // Run counter which if not null is incremented when the job is initially
    // queued, and decremented when the job completes.
    JobCounter* run_counter = nullptr;

    // Optional context for a job. Created on demand.
    std::unique_ptr<Context> context;

    // Arbitrary job-specific data.
    JobData data;
  };

  struct FiberState {
    // Fiber the state was switch from. This is not null when switching to a
    // waiting fiber, which then must mark this fiber as unused.
    Fiber prev_fiber = nullptr;

    // Wait counter which is set when a state goes into a Wait state.
    JobCounter* wait_counter = nullptr;

    // Fiber this state is for.
    Fiber fiber = nullptr;

    // Job this fiber is currently running.
    Job* job = nullptr;
  };

  // Work for a job thread: a job to run, or a waiting fiber to resume. Neither
  // is set once the job system has stopped.
  struct Work {
    Job* job = nullptr;
    FiberState* fiber = nullptr;
  };

  template <typename Type>
  using ConcurrentQueue = moodycamel::ConcurrentQueue<Type>;

  FiberJobSystem();
  bool Init(ValidatedContext context);

  // Queues a job to run, or the fiber states of jobs that are done waiting
  // (from JobCounter::Decrement()) to resume, and wakes threads to run them.
  void EnqueueJob(Job* job);
  void EnqueueFibers(absl::Span<void* const> states);

  // Waits for a job to run or a waiting fiber to resume, or for the job system
  // to stop.
  Work WaitForWork();

  // Runs a job on this fiber, and then destroys it.
  void RunJob(Fiber fiber, Job* job);

  // Removes a finished job from the jobs the system is running.
  void EndJob(Job* job);

  // Switches this fiber to the now-unblocked fiber in the specified state.
  void ResumeJobFiber(Fiber fiber, FiberState* state);

  // Must be called when switched to from a Wait call in another fiber to
  // complete the wait operation.
  void CompleteWait(Fiber fiber);

  // Main routine for a job fiber which runs jobs
  void JobMain(Fiber fiber);

  static std::string_view GetJobName(Job* job);

  bool set_fiber_names_ = false;
  absl::Duration shutdown_timeout_ = kDefaultShutdownTimeout;
  Clock* clock_ = nullptr;

  // All threads used to run jobs.
  std::vector<Thread> threads_;

  // Total number of fibers created. This only increases, as fibers are
  // recycled.
  std::atomic<int> total_fiber_count_;

  // Allocates used for job and fiber state.
  TsPoolAllocator job_allocator_;
  TsPoolAllocator fiber_allocator_;

  // The number of jobs that have been run and haven't yet finished.
  std::atomic<int> job_count_ = 0;

  // Set once the job system is being destroyed and all of its jobs have
  // finished, so the job threads exit.
  std::atomic<bool> stopped_ = false;

  // Every job that has been run and hasn't yet finished, kept only when job
  // names are, to name the jobs that keep the job system from being
  // destroyed.
  absl::Mutex named_jobs_mutex_;
  absl::flat_hash_set<Job*> named_jobs_ ABSL_GUARDED_BY(named_jobs_mutex_);

  // Signaled once for each job or fiber added to pending_jobs_ or
  // pending_fibers_ (see EnqueueJob() and EnqueueFibers()), and once for each
  // thread when the job system has stopped.
  moodycamel::LightweightSemaphore work_semaphore_;

  // Pending jobs waiting for a fiber to become available.
  ConcurrentQueue<Job*> pending_jobs_;

  // Pending fibers with an active job that are waiting for a thread to become
  // available.
  ConcurrentQueue<FiberState*> pending_fibers_;

  // Fibers that were created but are not currently in use.
  ConcurrentQueue<Fiber> unused_fibers_;
};

}  // namespace gb

#endif  // GB_JOB_FIBER_JOB_SYSTEM_H_
