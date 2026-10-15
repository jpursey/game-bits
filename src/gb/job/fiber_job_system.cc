// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/fiber_job_system.h"

#include <algorithm>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
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

  int thread_count = context.GetValue<int>(kKeyThreadCount);
  if (thread_count <= 0) {
    thread_count = std::max(GetMaxConcurrency() + thread_count, 1);
  }
  if (thread_count > kMaxThreadCount) {
    LOG(WARNING) << "Too many threads (" << thread_count
                 << ") for FiberJobSystem. Clamping to " << kMaxThreadCount
                 << " threads.";
    thread_count = kMaxThreadCount;
  }

  FiberOptions options;
  if (context.GetValue<bool>(kKeyPinThreads)) {
    options += FiberOption::kPinThreads;
  }
  if (set_fiber_names_) {
    options += FiberOption::kSetThreadName;
  }
  auto fiber_threads = CreateFiberThreads(
      thread_count, options, 0, this, +[](void* user_data) {
        auto* const job_system = static_cast<FiberJobSystem*>(user_data);
        const Fiber fiber = GetThisFiber();
        GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Starting fiber";
        job_system->SetThreadState();
        job_system->JobMain(fiber);
        // Nothing can happen as the job system may be in its destructor.
      });
  if (fiber_threads.empty()) {
    LOG(ERROR) << "No threads could be created to run FiberJobSystem.";
    return false;
  }

  threads_.reserve(fiber_threads.size());
  for (const auto& fiber_thread : fiber_threads) {
    threads_.push_back(fiber_thread.thread);
  }
  total_fiber_count_.store(static_cast<int>(fiber_threads.size()),
                           std::memory_order_release);
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
  work_semaphore_.signal(static_cast<int>(threads_.size()));

  for (auto thread : threads_) {
    JoinThread(thread);
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
  return static_cast<int>(threads_.size());
}

int FiberJobSystem::GetFiberCount() const {
  return total_fiber_count_.load(std::memory_order_acquire);
}

void FiberJobSystem::EnqueueJob(Job* job) {
  pending_jobs_.enqueue(job);
  work_semaphore_.signal();
}

void FiberJobSystem::EnqueueFibers(absl::Span<void* const> states) {
  for (void* waiter : states) {
    auto* const state = static_cast<FiberState*>(waiter);
    state->wait_counter = nullptr;
    pending_fibers_.enqueue(state);
  }
  work_semaphore_.signal(static_cast<int>(states.size()));
}

FiberJobSystem::Work FiberJobSystem::WaitForWork() {
  // Each signal is for one queued fiber or job, unless the job system has
  // stopped. The queue may not show a new entry right away, so this retries
  // until it does.
  work_semaphore_.wait();
  Work work;
  while (!stopped_.load(std::memory_order_acquire)) {
    if (pending_fibers_.try_dequeue(work.fiber) ||
        pending_jobs_.try_dequeue(work.job)) {
      break;
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
  unused_fibers_.enqueue(fiber);
  GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Exiting fiber";
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
  job->callback();
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
