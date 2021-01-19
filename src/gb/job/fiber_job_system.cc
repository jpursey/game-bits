// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/fiber_job_system.h"

#include <iostream>
#include <thread>

#include "absl/memory/memory.h"
#include "gb/thread/thread.h"
#include "glog/logging.h"

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
bool g_enable_fiber_job_system_logging_ = false;
#define GB_FIBER_JOB_SYSTEM_LOG \
  LOG_IF(INFO, g_enable_fiber_job_system_logging_) << "FiberJobSystem: "
#else  // GB_BUILD_ENABLE_JOB_LOGGING
#define GB_FIBER_JOB_SYSTEM_LOG \
  if (true)                     \
    ;                           \
  else                          \
    std::cout
#endif  // GB_BUILD_ENABLE_JOB_LOGGING

}  // namespace

void FiberJobSystem::SetVerboseLogging(bool enabled) {
#if GB_BUILD_ENABLE_JOB_LOGGING
  g_enable_fiber_job_system_logging_ = true;
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

  int thread_count = context.GetValue<int>(kKeyThreadCount);
  if (thread_count > kMaxThreadCount) {
    LOG(WARNING) << "Too many threads (" << thread_count
                 << ") requested for FiberJobSystem. Clamping to "
                 << kMaxThreadCount << " threads.";
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
    : running_(true),
      job_allocator_(1000, sizeof(Job)),
      fiber_allocator_(200, sizeof(FiberState)) {}

FiberJobSystem::~FiberJobSystem() {
  running_.store(false, std::memory_order_release);
  for (auto thread : threads_) {
    JoinThread(thread);
  }
  GB_JOB_CHECK(unused_fibers_.size_approx() ==
               total_fiber_count_.load(std::memory_order_acquire));
  GB_JOB_CHECK(pending_fibers_.size_approx() == 0);

  Job* job = nullptr;
  while (pending_jobs_.try_dequeue(job)) {
    job_allocator_.Delete(job);
  }
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
  while (running_.load(std::memory_order_acquire)) {
    FiberState* state = nullptr;

    if (pending_fibers_.try_dequeue(state)) {
      ResumeJobFiber(fiber, state);

      // Code may never get to this point. If it does, then fiber was
      // removed from the unused_fibers_ queue by a Wait call and must attempt
      // to wait the previous state.
      CompleteWait(fiber);
      continue;
    }

    if (Job * next_job; pending_jobs_.try_dequeue(next_job)) {
      // Create fiber state to track the running job.
      state = fiber_allocator_.New<FiberState>(this);
      state->fiber = fiber;
      state->job = next_job;
      if (set_fiber_names_) {
        SetFiberName(fiber, state->job->name);
      }
      GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Acquiring job "
                              << GetJobName(state->job);
    } else {
      // There is no work to do, so spin until the job system ends or a new job
      // is ready to run.
      std::this_thread::yield();
      continue;
    }

    // Set the fiber state for the running job.
    SetFiberData(fiber, state);

    // Run the job
    GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Running job "
                            << GetJobName(state->job) << " callback";
    state->job->callback();
    GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Completed job "
                            << GetJobName(state->job) << " callback";

    // Decrement run counter and unblock any waiting jobs.
    JobCounter* const counter = state->job->run_counter;
    JobCounter::Waiters waiters;
    if (counter != nullptr && counter->Decrement({}, waiters)) {
      for (void* waiter : waiters) {
        FiberState* const wait_state = static_cast<FiberState*>(waiter);
        wait_state->wait_counter = nullptr;
        pending_fibers_.enqueue(wait_state);
      }
    }

    // Clean up the job and fiber state.
    SetFiberData(fiber, nullptr);
    if (set_fiber_names_) {
      SetFiberName(fiber, "Idle Job Fiber");
    }
    job_allocator_.Delete(state->job);
    fiber_allocator_.Delete(state);
  }
  unused_fibers_.enqueue(fiber);
  GB_FIBER_JOB_SYSTEM_LOG << fiber << ": Exiting fiber";
}

bool FiberJobSystem::DoRun(std::string_view name, JobCounter* counter,
                           Callback<void()> callback) {
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
      static std::atomic<int> job_index(1);
      job->name = absl::StrCat(
          "Job-", job_index.fetch_add(1, std::memory_order_relaxed));
    }
  }
  GB_FIBER_JOB_SYSTEM_LOG << GetThisFiber() << ": Created job";
  job->run_counter = counter;
  job->callback = std::move(callback);
  pending_jobs_.enqueue(job);
  return true;
}

void FiberJobSystem::DoWait(JobCounter* counter) {
  const Fiber fiber = GetThisFiber();
  GB_JOB_CHECK(fiber != nullptr);
  if (counter == nullptr) {
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

}  // namespace gb
