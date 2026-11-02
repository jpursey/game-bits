// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/fiber_job_system.h"

#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/numeric/bits.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "gb/profile/profile_timer.h"
#include "gb/thread/fiber.h"
#include "gb/thread/thread.h"

namespace gb {

namespace {

// This may be set to CHECK or LOG_IF as needed. As there is overhead, the
// default is to DCHECK which compiles out in release builds.
#define GB_JOB_CHECK(test) DCHECK(test)
#define GB_JOB_CHECK_ALWAYS_RUN(action) \
  if (!(action))                        \
    DCHECK(false) << #action;           \
  else

#if GB_BUILD_ENABLE_JOB_LOGGING
std::atomic<bool> g_verbose_logging = false;
#define GB_FIBER_JOB_SYSTEM_LOG                                   \
  LOG_IF(INFO, g_verbose_logging.load(std::memory_order_relaxed)) \
      << "FiberJobSystem: "
#else  // GB_BUILD_ENABLE_JOB_LOGGING
#define GB_FIBER_JOB_SYSTEM_LOG LOG_IF(INFO, false)
#endif  // GB_BUILD_ENABLE_JOB_LOGGING

}  // namespace

void FiberJobSystem::SetVerboseLogging(bool enabled) {
#if GB_BUILD_ENABLE_JOB_LOGGING
  g_verbose_logging.store(enabled, std::memory_order_relaxed);
#endif  // GB_BUILD_ENABLE_JOB_LOGGING
}

std::unique_ptr<FiberJobSystem> FiberJobSystem::Create(
    CreateContract contract) {
  if (!contract.IsValid()) {
    return nullptr;
  }
  if (!SupportsFibers()) {
    return nullptr;
  }
  auto job_system = absl::WrapUnique(new FiberJobSystem);
  if (!job_system->Init(std::move(contract))) {
    return nullptr;
  }
  return job_system;
}

bool FiberJobSystem::Init(ValidatedContext context) {
  if (context.Exists<bool>(kKeySetFiberNames)) {
    set_fiber_names_ = context.GetValue<bool>(kKeySetFiberNames);
  }
#ifndef NDEBUG
  else {
    set_fiber_names_ = true;
  }
#endif  // NDEBUG

  shutdown_timeout_ = context.GetValue<absl::Duration>(kKeyShutdownTimeout);
  clock_ = context.GetPtr<Clock>();
  if (clock_ == nullptr) {
    clock_ = RealtimeClock::GetClock();
  }

  if (context.GetValue<bool>(kKeyProfileJobs)) {
    Profiler::Options options;
    if (auto* const given =
            context.GetPtr<Profiler::Options>(kKeyProfilerOptions);
        given != nullptr) {
      options = std::move(*given);
    }
    if (options.main != nullptr) {
      LOG(ERROR) << "FiberJobSystem profiler options cannot set a main "
                    "Profiler.";
      return false;
    }
    options.threadless = true;
    profiler_ = std::make_unique<Profiler>(std::move(options));
  }

  ThreadPlacementOptions placement_options = {
      .pinning = context.GetValue<ThreadPinning>(kKeyThreadPinning),
      .reserved_cores = context.GetValue<int>(kKeyReservedCores),
      .thread_count = context.GetValue<int>(kKeyThreadCount),
  };
  if (placement_options.thread_count < 0) {
    LOG(ERROR) << "FiberJobSystem thread count cannot be negative ("
               << placement_options.thread_count << ").";
    return false;
  }
  placement_ = PlaceThreads(placement_options);
  if (static_cast<int>(placement_.threads.size()) > kMaxThreadCount) {
    LOG(WARNING) << "Too many threads (" << placement_.threads.size()
                 << ") for FiberJobSystem. Clamping to " << kMaxThreadCount
                 << " threads.";
    placement_options.thread_count = kMaxThreadCount;
    placement_ = PlaceThreads(placement_options);
  }

  FiberOptions options;
  if (set_fiber_names_) {
    options += FiberOption::kSetThreadName;
  }
  auto fiber_threads = CreateFiberThreads(
      placement_.threads, options, 0, this, +[](void* user_data) {
        auto* const job_system = static_cast<FiberJobSystem*>(user_data);
        job_system->threads_created_.WaitForNotification();
        const Fiber fiber = GetThisFiber();
        GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Starting fiber";
        job_system->SetThreadState();
        job_system->AddThreadProfiler();
        job_system->JobMain(fiber);
        // Nothing can happen as the job system may be in its destructor.
      });
  if (fiber_threads.empty()) {
    LOG(ERROR) << "No threads could be created to run FiberJobSystem.";
    return false;
  }

  const int thread_count = static_cast<int>(fiber_threads.size());
  job_threads_ = std::vector<JobThread>(thread_count);
  for (int i = 0; i < thread_count; ++i) {
    job_threads_[i].thread = fiber_threads[i].thread;
  }
  placement_.threads.resize(thread_count);
  total_fiber_count_.store(thread_count, std::memory_order_release);
  if (profiler_ != nullptr) {
    thread_profilers_made_.emplace(thread_count);
  }
  threads_created_.Notify();
  if (profiler_ != nullptr) {
    thread_profilers_made_->Wait();
  }
  GB_FIBER_JOB_SYSTEM_LOG << "Job threads: "
                          << DescribeThreadPlacement(placement_);
  return true;
}

FiberJobSystem::FiberJobSystem()
    : job_allocator_(1000, sizeof(Job)),
      fiber_allocator_(200, sizeof(FiberState)) {}

FiberJobSystem::~FiberJobSystem() {
  // Wait for every job to finish. Nothing else can run a job once they have,
  // so the threads can then stop. A job that never finishes would hang the
  // destructor, so past the timeout this is fatal.
  const absl::Time deadline = clock_->Now() + shutdown_timeout_;
  while (job_count_.load(std::memory_order_acquire) > 0) {
    if (clock_->Now() >= deadline) {
      absl::MutexLock lock(&named_jobs_mutex_);
      LOG(FATAL) << "FiberJobSystem was destroyed with "
                 << job_count_.load(std::memory_order_acquire)
                 << " jobs that did not finish within " << shutdown_timeout_
                 << (set_fiber_names_ ? ": " : "")
                 << absl::StrJoin(named_jobs_, ", ",
                                  [](std::string* out, Job* job) {
                                    absl::StrAppend(out, GetJobName(job));
                                  });
    }
    clock_->SleepFor(absl::Milliseconds(1));
  }
  stopped_.store(true, std::memory_order_release);
  for (JobThread& job_thread : job_threads_) {
    job_thread.wake.signal();
  }
  for (JobThread& job_thread : job_threads_) {
    JoinThread(job_thread.thread);
  }
  GB_JOB_CHECK(unused_fibers_.size_approx() ==
               total_fiber_count_.load(std::memory_order_acquire));
  GB_JOB_CHECK(pending_fibers_.size_approx() == 0);
  GB_JOB_CHECK(pending_jobs_.size_approx() == 0);

  Fiber fiber = nullptr;
  while (unused_fibers_.try_dequeue(fiber)) {
    GB_JOB_CHECK(GetFiberData(fiber) == nullptr);
    DeleteFiber(fiber);
  }
}

std::string_view FiberJobSystem::GetJobName(Job* job) {
  if (job == nullptr) {
    return "null";
  }
  if (job->name.empty()) {
    return "anonymous";
  }
  return job->name;
}

int FiberJobSystem::GetThreadCount() const {
  return static_cast<int>(job_threads_.size());
}

int FiberJobSystem::GetFiberCount() const {
  return total_fiber_count_.load(std::memory_order_acquire);
}

const ThreadPlacement& FiberJobSystem::GetPlacement() const {
  return placement_;
}

Profiler* FiberJobSystem::GetProfiler() const { return profiler_.get(); }

int FiberJobSystem::GetIdleThreadCount() const {
  int count = 0;
  for (const JobThread& job_thread : job_threads_) {
    if (job_thread.waiting.load(std::memory_order_acquire)) {
      ++count;
    }
  }
  return count;
}

//------------------------------------------------------------------------------
// Queued work wakes the idle job thread that is first in placement order. A job
// thread that runs out of work sets its bit in idle_threads_ before checking
// queued_work_ once more, and whoever queues work increments queued_work_
// before reading idle_threads_. Every one of these is sequentially consistent,
// so either the job thread sees the work or the queuer sees the idle thread.
// Only the job thread sets its bit, and only whoever then clears it signals
// the thread's semaphore, so the thread's next wait takes every signal.
//------------------------------------------------------------------------------

void FiberJobSystem::EnqueueJob(Job* job) {
  pending_jobs_.enqueue(job);
  WakeThreadsForWork(1);
}

void FiberJobSystem::EnqueueFibers(absl::Span<void* const> states) {
  const int64_t ready_ticks = ReadProfilerTicks();
  for (void* waiter : states) {
    auto* const state = static_cast<FiberState*>(waiter);
    state->wait_counter = nullptr;
    state->ready_ticks = ready_ticks;
    pending_fibers_.enqueue(state);
  }
  WakeThreadsForWork(static_cast<int>(states.size()));
}

void FiberJobSystem::WakeThreadsForWork(int count) {
  queued_work_.fetch_add(count, std::memory_order_seq_cst);
  for (int i = 0; i < count; ++i) {
    if (!WakeIdleThread()) {
      break;
    }
  }
}

bool FiberJobSystem::WakeIdleThread() {
  uint64_t idle = idle_threads_.load(std::memory_order_seq_cst);
  while (idle != 0) {
    // Clear the lowest set bit, which is the first idle thread's.
    if (idle_threads_.compare_exchange_weak(idle, idle & (idle - 1),
                                            std::memory_order_seq_cst)) {
      job_threads_[absl::countr_zero(idle)].wake.signal();
      return true;
    }
  }
  return false;
}

bool FiberJobSystem::ClaimWork() {
  int count = queued_work_.load(std::memory_order_relaxed);
  while (count > 0) {
    if (queued_work_.compare_exchange_weak(count, count - 1,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed)) {
      return true;
    }
  }
  return false;
}

int FiberJobSystem::GetThisThreadIndex() const {
  const Thread thread = GetThisThread();
  const int thread_count = static_cast<int>(job_threads_.size());
  int index = 0;
  while (index < thread_count && job_threads_[index].thread != thread) {
    ++index;
  }
  CHECK(index < thread_count)
      << "FiberJobSystem: " << GetThreadName(thread) << " is not a job thread";
  return index;
}

FiberJobSystem::Work FiberJobSystem::WaitForWork() {
  Work work;
  while (!stopped_.load(std::memory_order_acquire)) {
    if (ClaimWork()) {
      // The queue may not show the claimed job or fiber right away, so this
      // retries until it does.
      while (!pending_fibers_.try_dequeue(work.fiber) &&
             !pending_jobs_.try_dequeue(work.job)) {
      }
      return work;
    }

    // If work was queued before this thread was marked idle, this wakes the
    // first idle thread to run it, which may be this one, rather than taking
    // it, so work always goes to the first idle thread.
    const int index = GetThisThreadIndex();
    JobThread& job_thread = job_threads_[index];
    idle_threads_.fetch_or(uint64_t{1} << index, std::memory_order_seq_cst);
    if (queued_work_.load(std::memory_order_seq_cst) > 0) {
      WakeIdleThread();
    }
    {
      ProfileIdleTimer idle_timer;
      job_thread.waiting.store(true, std::memory_order_release);
      job_thread.wake.wait();
      job_thread.waiting.store(false, std::memory_order_relaxed);
    }
  }
  return work;
}

void FiberJobSystem::ResumeJobFiber(Fiber fiber, FiberState* state) {
  GB_JOB_CHECK(fiber == GetThisFiber());

  // This fiber is now unused, as we will be switching to the pending
  // fiber.
  state->prev_fiber = fiber;

  // Associate this state with the fiber we are swiching to.
  GB_JOB_CHECK(GetFiberData(state->fiber) == state);

  // Switch this thread to the fiber.
  GB_FIBER_JOB_SYSTEM_LOG << state->prev_fiber << ": Resuming job "
                          << GetJobName(state->job);
  GB_JOB_CHECK_ALWAYS_RUN(SwitchToFiber(state->fiber));
}

void FiberJobSystem::CompleteWait(Fiber fiber) {
  GB_JOB_CHECK(fiber == GetThisFiber());
  while (true) {
    auto* state = static_cast<FiberState*>(SwapFiberData(fiber, nullptr));
    GB_JOB_CHECK(state != nullptr && state->wait_counter != nullptr);
    if (state->wait_counter->AddWaiter({}, state)) {
      return;
    }

    // The state is no longer waiting, so we need to immediately resume it.
    // (which the loop does).
    state->wait_counter = nullptr;
    state->ready_ticks = ReadProfilerTicks();
    ResumeJobFiber(fiber, state);

    // Code may never get to this point. If it does, then fiber was
    // removed from the unused_fibers_ queue by a Wait call and must attempt
    // to wait the previous state.
  }
}

void FiberJobSystem::JobMain(Fiber fiber) {
  GB_JOB_CHECK(fiber == GetThisFiber());
  while (true) {
    const Work work = WaitForWork();
    if (work.job != nullptr) {
      RunJob(fiber, work.job);
    } else if (work.fiber != nullptr) {
      ResumeJobFiber(fiber, work.fiber);

      // Code may never get to this point. If it does, then fiber was
      // removed from the unused_fibers_ queue by a Wait call and must attempt
      // to wait the previous state.
      CompleteWait(fiber);
    } else {
      break;
    }
  }

  // The thread ends once this returns, on whichever fiber it is on by now.
  job_threads_[GetThisThreadIndex()].profiler.reset();
  unused_fibers_.enqueue(fiber);
  GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Exiting fiber";
}

void FiberJobSystem::AddThreadProfiler() {
  if (profiler_ == nullptr) {
    return;
  }
  const int index = GetThisThreadIndex();
  job_threads_[index].profiler = std::make_unique<Profiler>(Profiler::Options{
      .main = profiler_.get(),
      .name = absl::StrCat("job thread ", index + 1),
  });
  thread_profilers_made_->DecrementCount();
}

ProfilePoint FiberJobSystem::FindJobPoint(std::string_view name) {
  if (name.empty()) {
    name = "unnamed job";
  }

  // Only a name's first run on each thread registers its point, which locks
  // the registry.
  thread_local absl::flat_hash_map<std::string, ProfilePoint> s_points;
  auto it = s_points.find(name);
  if (it == s_points.end()) {
    it = s_points.emplace(name, ProfilePoint(ProfilePoint::Kind::kJob, name))
             .first;
  }
  return it->second;
}

void FiberJobSystem::RunJob(Fiber fiber, Job* job) {
  // Create fiber state to track the running job.
  FiberState* state = fiber_allocator_.New<FiberState>();
  state->fiber = fiber;
  state->job = job;
  if (set_fiber_names_) {
    SetFiberName(fiber, job->name);
  }
  GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Acquiring job " << GetJobName(job);

  // Set the fiber state for the running job.
  SetFiberData(fiber, state);

  // Run the job. If it waits, it resumes on this fiber, possibly on another
  // thread.
  GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Running job " << GetJobName(job)
                          << " callback";
  if (profiler_ != nullptr) {
    ProfileJobTimer timer(*job->point, job->queued_ticks);
    job->callback();
  } else {
    job->callback();
  }
  GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Completed job " << GetJobName(job)
                          << " callback";

  // Reset job data and context, as waiting jobs may depend on this. For
  // consistency, context must always be destructed *after* job data.
  FreeJobData(job->data);
  job->context.reset();

  // Decrement run counter and unblock any waiting jobs.
  JobCounter* const counter = job->run_counter;
  JobCounter::Waiters waiters;
  if (counter != nullptr && counter->Decrement({}, waiters)) {
    EnqueueFibers(waiters);
  }

  // Clean up the job and fiber state.
  SetFiberData(fiber, nullptr);
  if (set_fiber_names_) {
    SetFiberName(fiber, "Idle Job Fiber");
  }
  fiber_allocator_.Delete(state);
  EndJob(job);
}

void FiberJobSystem::EndJob(Job* job) {
  if (set_fiber_names_) {
    absl::MutexLock lock(&named_jobs_mutex_);
    named_jobs_.erase(job);
  }
  job_allocator_.Delete(job);
  job_count_.fetch_sub(1, std::memory_order_release);
}

bool FiberJobSystem::DoRun(std::string_view name, JobCounter* counter,
                           Context* context,
                           absl::AnyInvocable<void()> callback) {
  Job* job = job_allocator_.New<Job>();
  if (job == nullptr) {
    return false;
  }
  if (counter != nullptr) {
    counter->Increment({});
  }
  if (set_fiber_names_) {
    if (!name.empty()) {
      job->name.assign(name.data(), name.size());
    } else {
      static std::atomic<int> s_job_index(1);
      job->name = absl::StrCat(
          "Job-", s_job_index.fetch_add(1, std::memory_order_relaxed));
    }
  }
  GB_FIBER_JOB_SYSTEM_LOG << GetThisFiber() << ": Created job "
                          << GetJobName(job);
  job->run_counter = counter;
  job->callback = std::move(callback);
  if (context != nullptr) {
    job->context = std::make_unique<Context>(std::move(*context));
  }
  job_count_.fetch_add(1, std::memory_order_relaxed);
  if (set_fiber_names_) {
    absl::MutexLock lock(&named_jobs_mutex_);
    named_jobs_.insert(job);
  }
  if (profiler_ != nullptr) {
    job->point = FindJobPoint(name);
    job->queued_ticks = profiler_->ReadTicks();
  }
  EnqueueJob(job);
  return true;
}

void FiberJobSystem::DoWait(JobCounter* counter) {
  const Fiber fiber = GetThisFiber();
  GB_JOB_CHECK(fiber != nullptr);
  if (counter == nullptr || counter->IsDone({})) {
    return;
  }

  FiberState* state = static_cast<FiberState*>(GetFiberData(fiber));
  GB_JOB_CHECK(state != nullptr);

  GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Waiting job "
                          << GetJobName(state->job);

  // Create a new fiber for this thread.
  FiberOptions options;
  if (set_fiber_names_) {
    options += FiberOption::kSetThreadName;
  }
  Fiber new_fiber = nullptr;
  if (!unused_fibers_.try_dequeue(new_fiber)) {
    total_fiber_count_.fetch_add(1, std::memory_order_release);
    new_fiber = CreateFiber(
        options, 0, this, +[](void* user_data) {
          auto* const system = static_cast<FiberJobSystem*>(user_data);
          const Fiber fiber = GetThisFiber();
          GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Starting fiber";
          system->CompleteWait(fiber);
          system->JobMain(fiber);
          // Nothing can happen as the job system may be in its destructor.
        });
  }
  GB_JOB_CHECK(new_fiber != nullptr);
  if (set_fiber_names_) {
    SetFiberName(new_fiber, "Idle Job Fiber");
  }

  // Set the state for the new fiber, so it can execute the wait safely (when
  // this fiber is no longer running).
  state->wait_counter = counter;
  SetFiberData(new_fiber, state);

  GB_JOB_CHECK_ALWAYS_RUN(SwitchToFiber(new_fiber));

  // We returned from our wait, so there MUST be a previous fiber (which is now
  // unused).
  GB_JOB_CHECK(state->prev_fiber != nullptr);
  unused_fibers_.enqueue(std::exchange(state->prev_fiber, nullptr));
  if (profiler_ != nullptr) {
    Profiler::AddResumeDelay(state->ready_ticks);
  }
}

Context& FiberJobSystem::DoGetContext() {
  const Fiber fiber = GetThisFiber();
  GB_JOB_CHECK(fiber != nullptr);

  FiberState* state = static_cast<FiberState*>(GetFiberData(fiber));
  GB_JOB_CHECK(state != nullptr);

  auto& context = state->job->context;
  if (context == nullptr) {
    context = std::make_unique<Context>();
  }
  return *context;
}

JobSystem::JobData& FiberJobSystem::DoGetJobData() {
  const Fiber fiber = GetThisFiber();
  GB_JOB_CHECK(fiber != nullptr);

  FiberState* state = static_cast<FiberState*>(GetFiberData(fiber));
  GB_JOB_CHECK(state != nullptr);

  return state->job->data;
}

}  // namespace gb
