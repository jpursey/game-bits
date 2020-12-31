// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>

#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "gb/thread/thread.h"
#include "glog/logging.h"

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

  absl::Mutex mutex;
  std::thread std_thread ABSL_GUARDED_BY(mutex);
  bool joined ABSL_GUARDED_BY(mutex) = false;
  bool exited ABSL_GUARDED_BY(mutex) = false;
  char name[kMaxThreadNameSize] ABSL_GUARDED_BY(mutex) = {};
};

namespace {

std::string ToString(Thread thread) ABSL_LOCKS_EXCLUDED(thread->mutex) {
  if (thread == nullptr) {
    return "null";
  }
  absl::MutexLock lock(&thread->mutex);
  return absl::StrFormat(
      "%s(%s)", thread->name,
      (thread->joined
           ? "joined"
           : (!thread->std_thread.joinable() ? "detached" : "active")));
}

void ThreadStartRoutine(void* param) {
  Thread thread = static_cast<Thread>(param);
  tls_this_thread = thread;

  GB_THREAD_LOG << "Starting thread " << ToString(thread);
  thread->thread_main(thread->user_data);
  GB_THREAD_LOG << "Exiting thread " << ToString(thread);

  bool detached = false;
  {
    absl::MutexLock lock(&thread->mutex);
    detached = !thread->std_thread.joinable() && !thread->joined;
    thread->exited = true;
  }
  if (detached) {
    delete thread;
  }
}

}  // namespace

void SetThreadVerboseLogging(bool enabled) {
#if GB_BUILD_ENABLE_THREAD_LOGGING
  g_enable_thread_logging_ = enabled;
#endif  // GB_BUILD_ENABLE_THREAD_LOGGING
}

int GetMaxConcurrency() {
  return std::max(static_cast<int>(std::thread::hardware_concurrency()), 1);
}

absl::Span<const uint64_t> GetHardwareThreadAffinities() { return {}; }

Thread CreateThread(uint64_t affinity, uint32_t stack_size, void* user_data,
                    ThreadMain thread_main) {
  Thread thread = new ThreadType(user_data, thread_main);
  {
    absl::MutexLock lock(&thread->mutex);
    thread->std_thread = std::thread(ThreadStartRoutine, thread);
  }
  return thread;
}

void JoinThread(Thread thread) {
  GB_THREAD_LOG << "Joining thread " << ToString(thread);
  std::thread std_thread;
  {
    absl::MutexLock lock(&thread->mutex);
    if (!thread->std_thread.joinable() || thread->joined) {
      LOG(ERROR)
          << "JoinThread called on thread that was already joined/detached";
      return;
    }
    thread->joined = true;
    std_thread = std::move(thread->std_thread);
  }
  std_thread.join();
  GB_THREAD_LOG << "Joined thread " << ToString(thread);
  delete thread;
}

void DetachThread(Thread thread) {
  GB_THREAD_LOG << "Detaching thread " << ToString(thread);
  bool exited = false;
  {
    absl::MutexLock lock(&thread->mutex);
    if (!thread->std_thread.joinable() || thread->joined) {
      LOG(ERROR)
          << "DetachThread called on thread that was already joined/detached";
      return;
    }
    thread->std_thread.detach();
    exited = thread->exited;
  }
  if (exited) {
    delete thread;
  }
}

uint64_t GetThreadAffinity(Thread thread) { return 0; }

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
}

Thread GetThisThread() { return tls_this_thread; }

int GetActiveThreadCount() { return g_active_thread_count; }

}  // namespace gb
