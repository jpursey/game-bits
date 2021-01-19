// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_JOB_FIBER_JOB_SYSTEM_H_
#define GB_JOB_FIBER_JOB_SYSTEM_H_

#include "absl/container/flat_hash_map.h"
#include "concurrentqueue.h"
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
  // be propagated only in debug builds.
  static inline constexpr const char* kKeySetFiberNames = "set_fiber_names";
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
    FiberState(FiberJobSystem* in_system) : system(in_system), idle(false) {}

    // Job system for this fiber.
    FiberJobSystem* const system;

    // Fiber the state was switch from. This is not null when switching to a
    // waiting fiber, which then must mark this fiber as unused.
    Fiber prev_fiber = nullptr;

    // Wait counter which is set when a state goes into a Wait state.
    JobCounter* wait_counter = nullptr;

    // Fiber this state is for.
    Fiber fiber = nullptr;

    // Job this fiber is currently running.
    Job* job = nullptr;

    // Set when the fiber becomes idle.
    std::atomic<bool> idle;
  };

  template <typename Type>
  using ConcurrentQueue = moodycamel::ConcurrentQueue<Type>;

  FiberJobSystem();
  bool Init(ValidatedContext context);

  // Switches this fiber to the now-unblocked fiber in the specified state.
  void ResumeJobFiber(Fiber fiber, FiberState* state);

  // Must be called when switched to from a Wait call in another fiber to
  // complete the wait operation.
  void CompleteWait(Fiber fiber);

  // Main routine for a job fiber which runs jobs
  void JobMain(Fiber fiber);

  static std::string_view GetJobName(Job* job);

  bool set_fiber_names_ = false;

  // True if the the fiber system is running (not being destructed).
  std::atomic<bool> running_;

  // All threads used to run jobs.
  std::vector<Thread> threads_;

  // Total number of fibers created. This only increases, as fibers are
  // recycled.
  std::atomic<int> total_fiber_count_;

  // Allocates used for job and fiber state.
  TsPoolAllocator job_allocator_;
  TsPoolAllocator fiber_allocator_;

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
