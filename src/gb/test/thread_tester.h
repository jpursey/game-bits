// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_TEST_THREAD_TESTER_H_
#define GB_TEST_THREAD_TESTER_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"

namespace gb {

// This class is used to simplify thread-safety tests.
//
// An instance of ThreadTester can be instantiated directly in a test or a test
// framework to support management of multiple threads which run concurrently,
// but are completed by the end of the test.
class ThreadTester {
 public:
  // A test function does work, and then returns true on success or false if the
  // test should fail (Complete() will return false).
  using TestFunction = std::function<bool()>;

  enum Result {
    kRunning,
    kFailure,
    kSuccess,
  };

  ThreadTester() = default;
  ThreadTester(const ThreadTester&) = delete;
  ThreadTester& operator=(const ThreadTester&) = delete;
  ~ThreadTester();

  static int MaxConcurrency() {
    return static_cast<int>(std::max(std::thread::hardware_concurrency(), 2u));
  }

  // Starts a thread running the specified function with the specified ID.
  void Run(const std::string& name, TestFunction func, int thread_count = 1);

  // Starts a thread that signals when it has completed.
  void RunThenSignal(int signal_id, const std::string& name, TestFunction func);

  // Starts a thread which runs the specified continuously until stopped by
  // signaling the loop with the specified ID.
  void RunLoop(int signal_id, const std::string& name, TestFunction func,
               int thread_count = 1);

  // Waits on or signals a notification. Notifications can only be signaled
  // once. Wait returns false if the timeout was reached before the signal
  // occurred.
  bool Wait(int signal_id, absl::Duration timeout = absl::Seconds(30));
  void Signal(int signal_id);

  // Signals all notifications, stops all loops, and joins all running threads.
  // If any test functions fail, this will return false.
  bool Complete();

  // Returns the result of a specific named run.
  Result GetRunResult(const std::string& name) const;

  // Returns result string of all threads.
  std::string GetResultString() const;

 private:
  struct ResultInfo {
    int running = 0;
    bool success = true;
  };

  using Notifications =
      absl::flat_hash_map<int, std::unique_ptr<absl::Notification>>;
  using Threads = std::vector<std::unique_ptr<std::thread>>;
  using Results = absl::flat_hash_map<std::string, ResultInfo>;

  void RunFunction(const std::string& name, const TestFunction& func)
      ABSL_LOCKS_EXCLUDED(mutex_);
  absl::Notification* GetNotification(int signal_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  Threads threads_ ABSL_GUARDED_BY(mutex_);
  Notifications signals_ ABSL_GUARDED_BY(mutex_);
  Results results_ ABSL_GUARDED_BY(mutex_);
  bool success_ ABSL_GUARDED_BY(mutex_) = true;
};

}  // namespace gb

#endif  // GB_TEST_THREAD_TESTER_H_
