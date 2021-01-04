// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/fiber_job_system.h"

#include <iostream>
#include <thread>

#include "absl/memory/memory.h"
#include "glog/logging.h"

namespace gb {

namespace {

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
  auto threads = CreateFiberThreads(
      thread_count, options, 0, this, +[](void* user_data) {
        auto* job_system = static_cast<FiberJobSystem*>(user_data);
        job_system->SetThreadState();
        job_system->JobMain();
        // Nothing can happen as the job system may be in its destructor.
      });
  if (threads.empty()) {
    LOG(ERROR) << "No threads could be created to run FiberJobSystem.";
    return false;
  }

  absl::MutexLock lock(&mutex_);
  total_thread_count_ = total_fiber_count_ = static_cast<int>(threads.size());
  return true;
}

FiberJobSystem::FiberJobSystem()
    : job_allocator_(1000, sizeof(Job)),
      fiber_allocator_(200, sizeof(FiberState)),
      pending_jobs_(1000),
      pending_fibers_(200),
      unused_fibers_(200) {}

FiberJobSystem::~FiberJobSystem() {
  absl::MutexLock lock(&mutex_);

  running_ = false;
  mutex_.Await(absl::Condition(this, &FiberJobSystem::HasNoFibersInUse));
  DCHECK(pending_fibers_.empty());
  DCHECK(idle_fibers_.empty());
  DCHECK(running_fibers_.empty());
  DCHECK(waiting_fibers_.empty());

  while (!pending_jobs_.empty()) {
    job_allocator_.Delete(pending_jobs_.front());
    pending_jobs_.pop();
  }
  while (!unused_fibers_.empty()) {
    // There is a race between JobMain exiting and the fiber to actually stop
    // running.
    while (IsFiberRunning(unused_fibers_.front())) {
      std::this_thread::yield();
    }
    DeleteFiber(unused_fibers_.front());
    unused_fibers_.pop();
  }
}

int FiberJobSystem::GetThreadCount() const {
  absl::ReaderMutexLock lock(&mutex_);
  return total_thread_count_;
}

int FiberJobSystem::GetFiberCount() const {
  absl::ReaderMutexLock lock(&mutex_);
  return total_fiber_count_;
}

bool FiberJobSystem::HasNoFibersInUse() const {
  return static_cast<int>(unused_fibers_.size()) == total_fiber_count_;
}

void FiberJobSystem::JobMain() {
  mutex_.Lock();
  const Fiber fiber = GetThisFiber();
  GB_FIBER_JOB_SYSTEM_LOG << "Starting fiber " << GetFiberName(fiber);
  while (running_) {
    FiberState* state = nullptr;

    if (!pending_fibers_.empty()) {
      // This fiber is now unused, as we will be switching to the pnding fiber.
      unused_fibers_.push(fiber);

      // The new fiber is no longer pending, but running.
      state = pending_fibers_.front();
      pending_fibers_.pop();
      running_fibers_.insert(state);

      // Switch this thread to the fiber.
      GB_FIBER_JOB_SYSTEM_LOG << "Resuming job " << state->job << " on fiber "
                              << GetFiberName(state->fiber);
      mutex_.Unlock();
      SwitchToFiber(state->fiber);

      // Code may never get to this point. If it does, then fiber was removed
      // from the unused_fibers_ queue and is now assigned to a thread.
      mutex_.Lock();
      continue;
    }

    if (!pending_jobs_.empty()) {
      // Create fiber state to track the running job.
      state = fiber_allocator_.New<FiberState>(this);
      state->fiber = fiber;
      state->job = pending_jobs_.front();
      pending_jobs_.pop();
      GB_FIBER_JOB_SYSTEM_LOG << "Acquiring job " << state->job << " on fiber "
                              << GetFiberName(fiber);
    } else {
      // There is no work to do, so we switch into an idle state waiting for the
      // job system to end or a new job is ready to run.
      state = fiber_allocator_.New<FiberState>(this);
      state->fiber = fiber;
      idle_fibers_.insert(state);
      GB_FIBER_JOB_SYSTEM_LOG << "Idling fiber " << GetFiberName(fiber);
      mutex_.Await(absl::Condition(
          +[](FiberState* state) {
            return state->job != nullptr || !state->system->running_;
          },
          state));
      if (!running_) {
        GB_FIBER_JOB_SYSTEM_LOG << "Ending idle fiber " << GetFiberName(fiber);
        idle_fibers_.erase(state);
        fiber_allocator_.Delete(state);
        break;
      }
      GB_FIBER_JOB_SYSTEM_LOG << "Resuming idle fiber " << GetFiberName(fiber)
                              << " with job " << state->job;
    }

    // This is now a running fiber!
    running_fibers_.insert(state);

    // Run the job
    GB_FIBER_JOB_SYSTEM_LOG << "Running job " << state->job
                            << " callback on fiber " << GetFiberName(fiber);
    mutex_.Unlock();
    state->job->callback();
    mutex_.Lock();
    GB_FIBER_JOB_SYSTEM_LOG << "Completed job " << state->job
                            << " callback on fiber " << GetFiberName(fiber);

    // Decrement run counter and unblock any waiting jobs.
    JobCounter* const counter = state->job->run_counter;
    if (counter != nullptr && counter->Decrement({}) == 0) {
      auto it = waiting_fibers_.find(counter);
      if (it != waiting_fibers_.end()) {
        for (auto* waiting_state : it->second) {
          pending_fibers_.push(waiting_state);
        }
        waiting_fibers_.erase(it);
      }
    }

    // Clean up the job and fiber state.
    running_fibers_.erase(state);
    job_allocator_.Delete(state->job);
    fiber_allocator_.Delete(state);
  }
  unused_fibers_.push(fiber);
  GB_FIBER_JOB_SYSTEM_LOG << "Exiting fiber " << GetFiberName(fiber);
  mutex_.Unlock();
}

bool FiberJobSystem::DoRun(JobCounter* counter, Callback<void()> callback) {
  absl::MutexLock lock(&mutex_);
  Job* job = job_allocator_.New<Job>();
  if (job == nullptr) {
    return false;
  }
  GB_FIBER_JOB_SYSTEM_LOG << "Created job " << job;
  if (counter != nullptr) {
    counter->Increment({});
  }
  job->run_counter = counter;
  job->callback = std::move(callback);
  if (idle_fibers_.empty()) {
    pending_jobs_.push(job);
  } else {
    auto it = idle_fibers_.begin();
    FiberState* state = *it;
    idle_fibers_.erase(it);
    state->job = job;
  }
  return true;
}

void FiberJobSystem::DoWait(JobCounter* counter) {
  const Fiber fiber = GetThisFiber();
  DCHECK(fiber != nullptr) << "Wait can only be called from within a job";
  if (counter == nullptr || counter->Get({}) == 0) {
    return;
  }

  {
    absl::MutexLock lock(&mutex_);
    FiberState* state = nullptr;
    for (auto* running_state : running_fibers_) {
      if (running_state->fiber == fiber) {
        state = running_state;
        break;
      }
    }
    CHECK(state != nullptr)
        << "Attempting to wait on a different fiber job system";

    GB_FIBER_JOB_SYSTEM_LOG << "Waiting job " << state->job << " on fiber "
                            << GetFiberName(state->fiber);

    // This fiber is now waiting, and a new fiber will be created.
    running_fibers_.erase(state);
    waiting_fibers_[counter].push_back(state);
    ++total_fiber_count_;
  }

  // Create a new fiber for this thread.
  Fiber new_fiber = CreateFiber(
      {}, 0, this, +[](void* user_data) {
        static_cast<FiberJobSystem*>(user_data)->JobMain();
        // Nothing can happen as the job system may be in its destructor.
      });
  CHECK(new_fiber != nullptr)
      << "Failed to create new fiber to replace waiting fiber on thread";
  CHECK(SwitchToFiber(new_fiber)) << "Failed to switch to new fiber";
}

}  // namespace gb
