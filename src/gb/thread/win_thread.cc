// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <algorithm>
#include <atomic>
#include <iostream>

#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "gb/base/unicode.h"
#include "gb/base/win_platform.h"
#include "gb/thread/thread.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <windows.h>

namespace gb {

namespace {

#if GB_BUILD_ENABLE_THREAD_LOGGING
bool g_enable_thread_logging_ = false;
#define GB_THREAD_LOG LOG_IF(INFO, g_enable_thread_logging_) << "Thread: "
#else  // GB_BUILD_ENABLE_THREAD_LOGGING
#define GB_THREAD_LOG \
  if (true)           \
    ;                 \
  else                \
    std::cout
#endif  // GB_BUILD_ENABLE_THREAD_LOGGING

thread_local Thread tls_this_thread = nullptr;
inline constexpr int kMaxThreadNameSize = 128;
std::atomic<int> g_thread_index = 0;
std::atomic<int> g_active_thread_count = 0;

absl::Span<const uint64_t> GetHardwareThreadAffinitiesImpl() {
  static uint64_t s_indices[64];
  static uint64_t s_count = 0;

  // Determine affinity mask
  uint64_t process_affinity_mask = 0;
  uint64_t system_affinity_mask = 0;
  if (!GetProcessAffinityMask(GetCurrentProcess(), &process_affinity_mask,
                              &system_affinity_mask)) {
    LOG(ERROR) << "Could not determine process thread affinity: "
               << GetWindowsError();
    return {};
  }

  if (process_affinity_mask == 0) {
    LOG(ERROR) << "Unsupported platform with process thread affinity in "
                  "multiple groups.";
    return {};
  }
  for (uint64_t i = 0; i < 64; ++i) {
    if (process_affinity_mask & (1ULL << i)) {
      s_indices[s_count++] = (1ULL << i);
    }
  }
  return absl::MakeConstSpan(&s_indices[0], s_count);
}

void SetWinThreadName(void* win_thread, std::string_view name) {
  std::u16string slow_thread_description;
  WCHAR fast_thread_description[kMaxThreadNameSize];
  const WCHAR* thread_description = fast_thread_description;
  int i = 0;
  for (char ch : name) {
    unsigned char u8ch = *reinterpret_cast<unsigned char*>(&ch);
    if (u8ch > 127) {
      thread_description = nullptr;
      break;
    }
    fast_thread_description[i++] = u8ch;
  }

  if (thread_description != nullptr) {
    fast_thread_description[i] = 0;
  } else {
    slow_thread_description = ToUtf16(name);
    thread_description =
        reinterpret_cast<const WCHAR*>(slow_thread_description.c_str());
  }

  HRESULT result =
      SetThreadDescription(static_cast<HANDLE>(win_thread), thread_description);
  if (FAILED(result)) {
    LOG(WARNING) << "Failed to set thread description to: " << name;
  }
}

}  // namespace

struct ThreadType {
  ThreadType(void* in_user_data, ThreadMain in_thread_main)
      : user_data(in_user_data), thread_main(in_thread_main) {
    std::snprintf(name, sizeof(name), "Thread-%d", ++g_thread_index);
    ++g_active_thread_count;
  }
  ~ThreadType() { --g_active_thread_count; }

  void* user_data = nullptr;
  ThreadMain thread_main = nullptr;
  uint64_t affinity = 0;

  absl::Mutex mutex;
  HANDLE win_thread ABSL_GUARDED_BY(mutex) = NULL;
  bool detached ABSL_GUARDED_BY(mutex) = false;
  bool joined ABSL_GUARDED_BY(mutex) = false;
  char name[kMaxThreadNameSize] ABSL_GUARDED_BY(mutex) = {};
};

namespace {

std::string ToString(Thread thread) ABSL_LOCKS_EXCLUDED(thread->mutex) {
  if (thread == nullptr) {
    return "null";
  }
  absl::MutexLock lock(&thread->mutex);
  return absl::StrFormat(
      "%s(%s:%p)", thread->name,
      (thread->detached ? "detached" : (thread->joined ? "joined" : "active")),
      thread->win_thread);
}

unsigned __stdcall ThreadStartRoutine(void* param) {
  Thread thread = static_cast<Thread>(param);
  HANDLE win_thread = NULL;
  tls_this_thread = thread;

  {
    absl::MutexLock lock(&thread->mutex);
    win_thread = thread->win_thread;
    SetWinThreadName(win_thread, thread->name);
  }

  GB_THREAD_LOG << "Starting thread " << ToString(thread);
  thread->thread_main(thread->user_data);
  GB_THREAD_LOG << "Exiting thread " << ToString(thread);

  bool detached = false;
  {
    absl::MutexLock lock(&thread->mutex);
    thread->win_thread = NULL;
    detached = thread->detached;
  }
  if (detached) {
    delete thread;
    ::CloseHandle(win_thread);
  }
  return 0;
}

}  // namespace

void SetThreadVerboseLogging(bool enabled) {
#if GB_BUILD_ENABLE_THREAD_LOGGING
  g_enable_thread_logging_ = enabled;
#endif  // GB_BUILD_ENABLE_THREAD_LOGGING
}

int GetMaxConcurrency() {
  return std::max(static_cast<int>(GetHardwareThreadAffinities().size()), 1);
}

absl::Span<const uint64_t> GetHardwareThreadAffinities() {
  static absl::Span<const uint64_t> s_affinities =
      GetHardwareThreadAffinitiesImpl();
  return s_affinities;
}

Thread CreateThread(uint64_t affinity, uint32_t stack_size, void* user_data,
                    ThreadMain thread_main) {
  Thread thread = new ThreadType(user_data, thread_main);
  HANDLE win_thread = reinterpret_cast<HANDLE>(
      _beginthreadex(nullptr, static_cast<unsigned>(stack_size),
                     ThreadStartRoutine, thread, CREATE_SUSPENDED, nullptr));
  if (win_thread == NULL) {
    LOG(ERROR) << "Failed to create thread: " << GetWindowsError();
    delete thread;
    return nullptr;
  }

  if (affinity != 0) {
    if (SetThreadAffinityMask(win_thread, affinity) != 0) {
      thread->affinity = affinity;
    } else {
      LOG(ERROR) << "Failed to set affinity for thread: " << GetWindowsError();
    }
  }

  {
    absl::MutexLock lock(&thread->mutex);
    thread->win_thread = win_thread;
  }

  ::ResumeThread(win_thread);
  return thread;
}

void JoinThread(Thread thread) {
  GB_THREAD_LOG << "Joining thread " << ToString(thread);
  HANDLE win_thread = NULL;
  {
    absl::MutexLock lock(&thread->mutex);
    if (thread->detached || thread->joined) {
      LOG(ERROR)
          << "JoinThread called on thread that was already joined/detached";
      return;
    }
    thread->joined = true;
    win_thread = thread->win_thread;
  }
  if (win_thread != NULL) {
    ::WaitForSingleObject(win_thread, INFINITE);
  }
  GB_THREAD_LOG << "Joined thread " << ToString(thread);
  delete thread;
  ::CloseHandle(win_thread);
}

void DetachThread(Thread thread) {
  GB_THREAD_LOG << "Detaching thread " << ToString(thread);
  HANDLE win_thread = NULL;
  {
    absl::MutexLock lock(&thread->mutex);
    if (thread->detached || thread->joined) {
      LOG(ERROR)
          << "DetachThread called on thread that was already joined/detached";
      return;
    }
    thread->detached = true;
    win_thread = thread->win_thread;
  }
  if (win_thread == NULL) {
    delete thread;
  }
}

uint64_t GetThreadAffinity(Thread thread) { return thread->affinity; }

std::string_view GetThreadName(Thread thread) {
  if (thread == nullptr) {
    return "null";
  }
  absl::MutexLock lock(&thread->mutex);
  return thread->name;
}

void SetThreadName(Thread thread, std::string_view name) {
  if (thread == nullptr) {
    return;
  }
  absl::MutexLock lock(&thread->mutex);
  size_t name_size = std::max<size_t>(name.size(), kMaxThreadNameSize - 1);
  std::memcpy(thread->name, name.data(), name_size);
  thread->name[name_size] = 0;
  if (thread->win_thread != NULL) {
    SetWinThreadName(thread->win_thread, thread->name);
  }
}

Thread GetThisThread() { return tls_this_thread; }

int GetActiveThreadCount() { return g_active_thread_count; }

}  // namespace gb
