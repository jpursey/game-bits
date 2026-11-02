// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_JOB_FIBER_JOB_SYSTEM_H_
#define GB_JOB_FIBER_JOB_SYSTEM_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "concurrentqueue.h"
#include "gb/alloc/pool_allocator.h"
#include "gb/base/clock.h"
#include "gb/base/context.h"
#include "gb/base/validated_context.h"
#include "gb/job/job_system.h"
#include "gb/job/job_types.h"
#include "gb/profile/profile_point.h"
#include "gb/profile/profiler.h"
#include "gb/thread/thread_placement.h"
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
// ready to run. A job ready to run wakes the idle job thread that is first in
// GetPlacement(), so while job threads are pinned, jobs start on the fastest
// idle core.
//
// This class is thread-safe.
class FiberJobSystem : public JobSystem {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // OPTIONAL: The number of threads the job system runs jobs on (max
  // concurrency), or zero (the default) for one on each hardware thread, or on
  // each core if they are pinned to cores (see kConstraintThreadPinning), that
  // isn't reserved (see kConstraintReservedCores). It is never more than
  // kMaxThreadCount. If it is negative, Create() fails.
  static inline constexpr int kMaxThreadCount = 64;
  static inline constexpr const char* kKeyThreadCount = "thread_count";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintThreadCount, kInOptional, int,
                                     kKeyThreadCount);

  // OPTIONAL: What each job thread is pinned to (see ThreadPinning): a hardware
  // thread of its own (the default), a core of its own, or nothing. Pinned job
  // threads take the fastest cores first. If there are more job threads than
  // hardware threads (or cores), none of them are pinned.
  static inline constexpr const char* kKeyThreadPinning = "thread_pinning";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintThreadPinning,
                                             kInOptional, ThreadPinning,
                                             kKeyThreadPinning,
                                             ThreadPinning::kHardwareThread);

  // OPTIONAL: The number of the fastest cores to leave without job threads, for
  // other work such as a program's main thread (zero by default). At least one
  // core is always left for job threads, and GetPlacement() reports which
  // cores are reserved. Job threads that aren't pinned (ThreadPinning::kNone)
  // may still run on reserved cores, so then this only reduces their number.
  static inline constexpr const char* kKeyReservedCores = "reserved_cores";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintReservedCores,
                                             kInOptional, int,
                                             kKeyReservedCores, 0);

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

  // OPTIONAL: If set to true, the job system profiles the jobs it runs (false
  // by default). It then has a main Profiler of its own (see GetProfiler()),
  // and each job thread has a Profiler joined to it, named "job thread 1" to
  // "job thread N" in placement order. Each job is timed as a job point named
  // after the job, or "unnamed job", with its start and resume delays, and each
  // job thread's waits for work are its idle time (see gb/profile/profiler.h).
  // Job code can time frames with ProfileFrame, and a frame can wait on other
  // jobs, ending on another job thread. Jobs can't make Profilers of their own,
  // as a thread has at most one.
  //
  // Each distinct job name registers a job point the first time a job with it
  // is run, so job names must come from a fixed set: points are limited to
  // kMaxProfilePoints, and registering more CHECK-fails, as does a job name
  // that names another kind of point.
  static inline constexpr const char* kKeyProfileJobs = "profile_jobs";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintProfileJobs,
                                             kInOptional, bool, kKeyProfileJobs,
                                             false);

  // OPTIONAL: The options for the main Profiler, used with
  // kConstraintProfileJobs: its budget, slow frame callback, and a
  // FakeProfilerCpu for tests, which the job threads' Profilers share. Create()
  // moves them out of the context, and sets `threadless`. If they set `main`,
  // Create() fails.
  static inline constexpr const char* kKeyProfilerOptions = "profiler_options";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintProfilerOptions, kInOptional,
                                     Profiler::Options, kKeyProfilerOptions);

  using CreateContract =
      ContextContract<kConstraintThreadCount, kConstraintThreadPinning,
                      kConstraintReservedCores, kConstraintSetFiberNames,
                      kConstraintShutdownTimeout, kConstraintClock,
                      kConstraintProfileJobs, kConstraintProfilerOptions>;

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

  // Returns where each job thread runs (see DescribeThreadPlacement() to log
  // it).
  const ThreadPlacement& GetPlacement() const;

  // Returns the number of job threads waiting for work. Once it equals
  // GetThreadCount(), every job thread is idle, so the next job runs on the
  // first job thread in GetPlacement().
  int GetIdleThreadCount() const;

  // Returns the main Profiler of the jobs, or null if kConstraintProfileJobs
  // isn't set. Any thread may read its report (see Profiler::GetReport()). A
  // thread outside the job system may join it (see Profiler::Options::main),
  // but its Profiler must be destroyed before the job system.
  Profiler* GetProfiler() const;

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

    // The job's point, and when it was queued (see Profiler::ReadTicks()), set
    // only when profiling.
    std::optional<ProfilePoint> point;
    int64_t queued_ticks = 0;
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

    // When the wait was over (see Profiler::ReadTicks()), set only when
    // profiling.
    int64_t ready_ticks = 0;
  };

  // Work for a job thread: a job to run, or a waiting fiber to resume. Neither
  // is set once the job system has stopped.
  struct Work {
    Job* job = nullptr;
    FiberState* fiber = nullptr;
  };

  // A thread that runs jobs. Each is on its own cache line, as other threads
  // signal its semaphore while it waits on it.
  struct alignas(ABSL_CACHELINE_SIZE) JobThread {
    Thread thread = nullptr;

    // Signaled to wake the thread when it is idle (see idle_threads_), and
    // when the job system stops.
    moodycamel::LightweightSemaphore wake;

    // Set while the thread waits on `wake`, past its last check for work.
    std::atomic<bool> waiting = false;

    // Joined to profiler_ while the thread runs, when profiling. It is made
    // and destroyed on the thread, as a fiber can move between threads.
    std::unique_ptr<Profiler> profiler;
  };

  template <typename Type>
  using ConcurrentQueue = moodycamel::ConcurrentQueue<Type>;

  FiberJobSystem();
  bool Init(ValidatedContext context);

  // Queues a job to run, or the fiber states of jobs that are done waiting
  // (from JobCounter::Decrement()) to resume, and wakes threads to run them.
  void EnqueueJob(Job* job);
  void EnqueueFibers(absl::Span<void* const> states);

  // Counts `count` jobs or fibers just queued, and wakes up to that many idle
  // job threads to run them.
  void WakeThreadsForWork(int count);

  // Wakes the idle job thread that is first in placement order, returning
  // false if no job thread is idle.
  bool WakeIdleThread();

  // Claims one of the jobs or fibers that are queued, returning false if there
  // are none.
  bool ClaimWork();

  // Returns the index in job_threads_ of the job thread this is running on.
  int GetThisThreadIndex() const;

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

  // Makes the calling job thread's Profiler, when profiling.
  void AddThreadProfiler();

  // Returns Profiler::ReadTicks() from profiler_, or zero when not profiling.
  int64_t ReadProfilerTicks() const {
    return profiler_ != nullptr ? profiler_->ReadTicks() : 0;
  }

  // Returns the job point for a job named `name`, registering it the first
  // time.
  static ProfilePoint FindJobPoint(std::string_view name);

  static std::string_view GetJobName(Job* job);

  bool set_fiber_names_ = false;
  absl::Duration shutdown_timeout_ = kDefaultShutdownTimeout;
  Clock* clock_ = nullptr;

  // The main Profiler of the jobs, when profiling (see GetProfiler()). It is
  // destroyed after the job threads' Profilers, which join it.
  std::unique_ptr<Profiler> profiler_;

  // All threads used to run jobs, and where each runs, in placement order.
  std::vector<JobThread> job_threads_;
  ThreadPlacement placement_;

  // Notified once job_threads_ holds every job thread, which the job threads
  // wait for before they run jobs, so they can find their own index.
  absl::Notification threads_created_;

  // When profiling, counts down as each job thread makes its Profiler, which
  // Create() waits for.
  std::optional<absl::BlockingCounter> thread_profilers_made_;

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

  // The number of jobs and fibers in pending_jobs_ and pending_fibers_ that no
  // job thread has claimed yet, and a bit for each idle job thread, by its
  // index in job_threads_ (see WaitForWork()). Every queuer and idle thread
  // uses both, so they share a cache line of their own, which pending_jobs_
  // ends.
  alignas(ABSL_CACHELINE_SIZE) std::atomic<int> queued_work_ = 0;
  static_assert(kMaxThreadCount <= 64);
  std::atomic<uint64_t> idle_threads_ = 0;

  // Pending jobs waiting for a fiber to become available.
  alignas(ABSL_CACHELINE_SIZE) ConcurrentQueue<Job*> pending_jobs_;

  // Pending fibers with an active job that are waiting for a thread to become
  // available.
  ConcurrentQueue<FiberState*> pending_fibers_;

  // Fibers that were created but are not currently in use.
  ConcurrentQueue<Fiber> unused_fibers_;
};

}  // namespace gb

#endif  // GB_JOB_FIBER_JOB_SYSTEM_H_
