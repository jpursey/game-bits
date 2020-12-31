// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <cstdio>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/notification.h"
#include "absl/types/span.h"
#include "gb/base/win_platform.h"
#include "gb/thread/fiber.h"
#include "gb/thread/thread.h"
#include "glog/logging.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <windows.h>

namespace gb {

namespace {

#if GB_BUILD_ENABLE_THREAD_LOGGING
bool g_enable_fiber_logging_ = false;
#define GB_FIBER_LOG LOG_IF(INFO, g_enable_fiber_logging_) << "Fiber: "
#else  // GB_BUILD_ENABLE_THREAD_LOGGING
#define GB_FIBER_LOG \
  if (true)          \
    ;                \
  else               \
    std::cout
#endif  // GB_BUILD_ENABLE_THREAD_LOGGING

using WinFiber = void*;

inline constexpr int kMaxFiberNameSize = 128;
std::atomic<int> g_fiber_index = 0;
std::atomic<int> g_running_count = 0;

}  // namespace

struct FiberType {
  FiberType(void* in_user_data, FiberMain in_fiber_main)
      : user_data(in_user_data), fiber_main(in_fiber_main) {
    std::snprintf(name, sizeof(name), "Fiber-%d", ++g_fiber_index);
  }

  void* user_data = nullptr;
  FiberMain fiber_main = nullptr;

  absl::Mutex mutex;
  WinFiber thread_win_fiber ABSL_GUARDED_BY(mutex) = nullptr;
  WinFiber win_fiber ABSL_GUARDED_BY(mutex) = nullptr;
  char name[kMaxFiberNameSize] ABSL_GUARDED_BY(mutex) = {};
};

namespace {

std::string ToString(Fiber fiber) ABSL_LOCKS_EXCLUDED(fiber->mutex) {
  if (fiber == nullptr) {
    return "null";
  }
  absl::MutexLock lock(&fiber->mutex);
  return absl::StrFormat("%s(%s:%p)", fiber->name,
                         (fiber->win_fiber != 0 ? "active" : "complete"),
                         fiber->thread_win_fiber);
}

// Temporary data used when starting a thread-based fiber.
struct FiberThreadStartInfo {
  FiberThreadStartInfo() = default;
  HANDLE win_thread = NULL;
  void* user_data = nullptr;
  FiberMain fiber_main = nullptr;
  Fiber fiber = nullptr;
  WinFiber thread_win_fiber = nullptr;
  absl::Notification started;
  DWORD error = ERROR_SUCCESS;
};

void RunFiberMain(Fiber fiber) {
  fiber->fiber_main(fiber->user_data);

  // fiber_main may never return. This would happen if the fiber switches
  // to a different fiber and then never switches back. In this case,
  // SwitchToFiber will set this fiber to not be running. If we do get here,
  // then we need clean up the fiber state and switch to a thread-based fiber to
  // shutdown (ending the thread).
  GB_FIBER_LOG << "Exiting fiber " << ToString(fiber);
  fiber->mutex.Lock();
  WinFiber thread_win_fiber = std::exchange(fiber->thread_win_fiber, nullptr);
  fiber->win_fiber = nullptr;
  --g_running_count;
  fiber->mutex.Unlock();

  ::SwitchToFiber(thread_win_fiber);
}

void FiberStartRoutine(void* param) {
  Fiber fiber = static_cast<Fiber>(param);
  GB_FIBER_LOG << "Starting fiber " << ToString(fiber);
  RunFiberMain(fiber);
  // Never gets here, RunFiberMain never returns.
}

void FiberStartRoutineFromThread(void* param) {
  auto* start_info = static_cast<FiberThreadStartInfo*>(param);
  Fiber fiber = start_info->fiber;

  // Notify *after* setting the thread to running, so there is no race condition
  // on 'running' being true.
  fiber->mutex.Lock();
  ++g_running_count;
  fiber->thread_win_fiber = start_info->thread_win_fiber;
  fiber->user_data = start_info->user_data;
  fiber->fiber_main = start_info->fiber_main;
  fiber->mutex.Unlock();
  GB_FIBER_LOG << "Attached thread to fiber " << ToString(fiber);
  start_info->error = ERROR_SUCCESS;
  start_info->started.Notify();  // start_info is deleted after this call.
  RunFiberMain(fiber);
  // Never gets here, RunFiberMain never returns.
}

unsigned __stdcall FiberThreadStartRoutine(void* param) {
  auto* start_info = static_cast<FiberThreadStartInfo*>(param);
  HANDLE win_thread = start_info->win_thread;
  WinFiber thread_win_fiber =
      ::ConvertThreadToFiberEx(param, FIBER_FLAG_FLOAT_SWITCH);
  if (thread_win_fiber == nullptr) {
    start_info->error = GetLastError();
    start_info->started.Notify();  // start_info is deleted after this call.
    return 1;
  }
  start_info->thread_win_fiber = thread_win_fiber;

  GB_FIBER_LOG << "Started thread " << thread_win_fiber;

  // Switch to the fiber specified in the start info.
  Fiber fiber = start_info->fiber;
  WinFiber next_win_fiber;
  fiber->mutex.Lock();
  next_win_fiber = fiber->win_fiber;
  fiber->mutex.Unlock();
  ::SwitchToFiber(next_win_fiber);

  // At this point, start_info is long gone and is no longer usable (or
  // relevant). Converting the fiber back to a thread will clean up any fiber
  // state.
  ::ConvertFiberToThread();
  GB_FIBER_LOG << "Exiting thread " << thread_win_fiber;
  ::CloseHandle(win_thread);
  return 0;
}

}  // namespace

bool SupportsFibers() { return true; }

void SetFiberVerboseLogging(bool enabled) {
#if GB_BUILD_ENABLE_THREAD_LOGGING
  g_enable_fiber_logging_ = enabled;
#endif  // GB_BUILD_ENABLE_THREAD_LOGGING
}

std::vector<Fiber> CreateFiberThreads(int thread_count, bool pin_threads,
                                      uint32_t stack_size, void* user_data,
                                      FiberMain fiber_main) {
  auto affinities = GetHardwareThreadAffinities();
  const int max_concurrency = static_cast<int>(affinities.size());
  if (thread_count <= 0) {
    thread_count = max_concurrency + thread_count;
    if (thread_count <= 0) {
      thread_count = 1;
    }
  }
  if (pin_threads && thread_count > max_concurrency) {
    pin_threads = false;
  }
  GB_FIBER_LOG << "Creating " << thread_count << " fiber threads of stack size "
               << stack_size << " that are "
               << (pin_threads ? "pinned" : "not pinned");

  std::vector<Fiber> fibers;
  fibers.reserve(thread_count);
  for (int i = 0; i < thread_count; ++i) {
    // Unfortunately, the Windows fiber library seems to have some undocumented
    // requirements that prevent safely exiting a thread from anything other
    // than the fiber that was originally converted from a thread. To accomodate
    // this, we create two fibers: first, the fiber which is returned to the
    // caller, and second one converted from the thread which is switched back
    // to when any other executing fiber on the thread exits.
    FiberThreadStartInfo start_info;
    start_info.user_data = user_data;
    start_info.fiber_main = fiber_main;
    start_info.fiber =
        gb::CreateFiber(stack_size, &start_info, FiberStartRoutineFromThread);
    if (start_info.fiber == nullptr) {
      break;
    }
    start_info.win_thread = reinterpret_cast<HANDLE>(
        _beginthreadex(nullptr, 4096, FiberThreadStartRoutine, &start_info,
                       CREATE_SUSPENDED, nullptr));
    if (start_info.win_thread == NULL) {
      LOG(ERROR) << "Failed to create fiber thread " << i << ": "
                 << GetWindowsError();
      gb::DeleteFiber(start_info.fiber);
      break;
    }
    if (pin_threads) {
      if (SetThreadAffinityMask(start_info.win_thread, affinities[i]) == 0) {
        LOG(ERROR) << "Failed to set affinity for fiber thread " << i << ": "
                   << GetWindowsError();
      }
    }
    ::ResumeThread(start_info.win_thread);

    start_info.started.WaitForNotification();
    if (start_info.error != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to convert fiber thread " << i
                 << " to fiber: " << GetWindowsError(start_info.error);
      gb::DeleteFiber(start_info.fiber);
      break;
    }
    fibers.emplace_back(start_info.fiber);
  }
  return fibers;
}

Fiber CreateFiber(uint32_t stack_size, void* user_data, FiberMain fiber_main) {
  Fiber fiber = new FiberType(user_data, fiber_main);
  fiber->mutex.Lock();
  fiber->win_fiber = ::CreateFiberEx(stack_size, 0, FIBER_FLAG_FLOAT_SWITCH,
                                     FiberStartRoutine, fiber);
  if (fiber->win_fiber == nullptr) {
    fiber->mutex.Unlock();
    LOG(ERROR) << "Failed to create fiber: " << GetWindowsError();
    delete fiber;
    return nullptr;
  }
  fiber->mutex.Unlock();
  GB_FIBER_LOG << "Created fiber " << ToString(fiber);
  return fiber;
}

void DeleteFiber(Fiber fiber) {
  CHECK(fiber != nullptr) << "Cannot delete an invalid fiber";
  GB_FIBER_LOG << "Deleting fiber " << ToString(fiber);
  fiber->mutex.Lock();
  CHECK(fiber->thread_win_fiber == nullptr) << "Cannot delete a running fiber";
  if (fiber->win_fiber != nullptr) {
    ::DeleteFiber(fiber->win_fiber);
    fiber->win_fiber = nullptr;
  }
  fiber->mutex.Unlock();
  delete fiber;
}

bool SwitchToFiber(Fiber fiber) {
  Fiber current_fiber = GetThisFiber();
  if (current_fiber == nullptr) {
    return false;
  }

  current_fiber->mutex.Lock();
  WinFiber thread_win_fiber = current_fiber->thread_win_fiber;
  current_fiber->mutex.Unlock();

  WinFiber win_fiber;
  fiber->mutex.Lock();
  win_fiber = fiber->win_fiber;
  if (fiber->thread_win_fiber != nullptr || win_fiber == nullptr) {
    fiber->mutex.Unlock();
    return false;
  }
  fiber->thread_win_fiber = thread_win_fiber;
  fiber->mutex.Unlock();

  GB_FIBER_LOG << "Switching thread from fiber " << ToString(current_fiber)
               << " to fiber " << ToString(fiber);

  current_fiber->mutex.Lock();
  current_fiber->thread_win_fiber = nullptr;
  current_fiber->mutex.Unlock();

  ::SwitchToFiber(win_fiber);
  return true;
}

std::string_view GetFiberName(Fiber fiber) {
  if (fiber == nullptr) {
    return "null";
  }
  absl::MutexLock lock(&fiber->mutex);
  return fiber->name;
}

void SetFiberName(Fiber fiber, std::string_view name) {
  if (fiber == nullptr) {
    return;
  }
  absl::MutexLock lock(&fiber->mutex);
  size_t name_size = std::max<size_t>(name.size(), kMaxFiberNameSize - 1);
  std::memcpy(fiber->name, name.data(), name_size);
  fiber->name[name_size] = 0;
}

Fiber GetThisFiber() {
  if (!::IsThreadAFiber()) {
    return nullptr;
  }
  return static_cast<Fiber>(::GetFiberData());
}

bool IsFiberRunning(Fiber fiber) {
  if (fiber == nullptr) {
    return false;
  }
  absl::MutexLock lock(&fiber->mutex);
  return fiber->thread_win_fiber != nullptr;
}

int GetRunningFiberCount() { return g_running_count; }

}  // namespace gb
