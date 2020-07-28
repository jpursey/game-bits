// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/test/thread_tester.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "glog/logging.h"

namespace gb {

absl::Notification* ThreadTester::GetNotification(int signal_id) {
  auto& notification = signals_[signal_id];
  if (notification == nullptr) {
    notification = std::make_unique<absl::Notification>();
  }
  return notification.get();
}

void ThreadTester::RunFunction(const std::string& name,
                               const TestFunction& func) {
  const bool success = func();
  absl::MutexLock lock(&mutex_);
  auto& result_info = results_[name];
  result_info.success = result_info.success && success;
  result_info.running -= 1;
  success_ = success_ && success;
}

ThreadTester::~ThreadTester() {
  {
    absl::MutexLock lock(&mutex_);
    if (threads_.empty()) {
      return;
    }
  }
  LOG(ERROR) << "Threads not completed.";
  Complete();
}

void ThreadTester::Run(const std::string& name, TestFunction func,
                       int thread_count) {
  absl::MutexLock lock(&mutex_);
  results_[name].running += thread_count;
  for (int i = 0; i < thread_count; ++i) {
    threads_.emplace_back(std::make_unique<std::thread>(
        [this, name, func]() { RunFunction(name, func); }));
  }
}

void ThreadTester::RunThenSignal(int signal_id, const std::string& name,
                                 TestFunction func) {
  absl::MutexLock lock(&mutex_);
  results_[name].running += 1;
  auto* notification = GetNotification(signal_id);
  threads_.emplace_back(
      std::make_unique<std::thread>([this, name, func, notification]() {
        RunFunction(name, func);
        notification->Notify();
      }));
}

void ThreadTester::RunLoop(int signal_id, const std::string& name,
                           TestFunction func, int thread_count) {
  absl::Notification* notification = nullptr;
  {
    absl::MutexLock lock(&mutex_);
    notification = GetNotification(signal_id);
  }
  auto loop_func = [func, notification]() {
    while (!notification->HasBeenNotified()) {
      if (!func()) {
        return false;
      }
    }
    return true;
  };
  Run(name, loop_func, thread_count);
}

bool ThreadTester::Wait(int signal_id, absl::Duration timeout) {
  absl::Notification* notification = nullptr;
  {
    absl::MutexLock lock(&mutex_);
    notification = GetNotification(signal_id);
  }
  return notification->WaitForNotificationWithTimeout(timeout);
}

void ThreadTester::Signal(int signal_id) {
  absl::MutexLock lock(&mutex_);
  auto* notification = GetNotification(signal_id);
  if (!notification->HasBeenNotified()) {
    notification->Notify();
  }
}

bool ThreadTester::Complete() {
  Threads threads;
  {
    absl::MutexLock lock(&mutex_);
    for (const auto& signal : signals_) {
      if (!signal.second->HasBeenNotified()) {
        signal.second->Notify();
      }
    }
    std::swap(threads_, threads);
  }

  for (auto& thread : threads) {
    thread->join();
  }

  absl::MutexLock lock(&mutex_);
  return success_;
}

ThreadTester::Result ThreadTester::GetRunResult(const std::string& name) const {
  absl::MutexLock lock(&mutex_);
  auto it = results_.find(name);
  if (it == results_.end()) {
    LOG(ERROR) << "Requested non-existent run result: \"" << name << "\"";
    return kFailure;
  }
  auto& result_info = it->second;
  if (!result_info.success) {
    return kFailure;
  }
  if (result_info.running > 0) {
    return kRunning;
  }
  return kSuccess;
}

std::string ThreadTester::GetResultString() const {
  absl::MutexLock lock(&mutex_);
  std::vector<std::string> results;
  for (const auto& info : results_) {
    if (!info.second.success) {
      results.push_back(absl::StrCat(info.first, " failure"));
    } else if (info.second.running > 0) {
      results.push_back(absl::StrCat(info.first, " running"));
    } else {
      results.push_back(absl::StrCat(info.first, " success"));
    }
  }
  return absl::StrJoin(results, ", ");
}

}  // namespace gb
