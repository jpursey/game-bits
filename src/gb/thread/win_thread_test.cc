// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "absl/synchronization/notification.h"
#include "gb/thread/thread.h"
#include "gtest/gtest.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb {
namespace {

struct ExitState {
  ExitState() = default;
  DWORD thread_id = 0;
  absl::Notification started;
};

void SignalStarted(void* user_data) {
  auto* state = static_cast<ExitState*>(user_data);
  state->thread_id = ::GetCurrentThreadId();
  state->started.Notify();
}

// Creates a thread and waits until the operating system thread has exited.
Thread CreateExitedThread() {
  ExitState state;
  Thread thread = CreateThread(0, 0, &state, SignalStarted);
  EXPECT_NE(thread, nullptr);
  state.started.WaitForNotification();
  HANDLE os_thread = ::OpenThread(SYNCHRONIZE, FALSE, state.thread_id);
  EXPECT_NE(os_thread, nullptr);
  ::WaitForSingleObject(os_thread, INFINITE);
  ::CloseHandle(os_thread);
  return thread;
}

int GetHandleCount() {
  DWORD count = 0;
  ::GetProcessHandleCount(::GetCurrentProcess(), &count);
  return static_cast<int>(count);
}

TEST(WinThreadTest, JoinExitedThreadClosesItsHandle) {
  JoinThread(CreateExitedThread());

  const int handle_count = GetHandleCount();
  JoinThread(CreateExitedThread());
  EXPECT_EQ(GetHandleCount(), handle_count);
}

TEST(WinThreadTest, DetachExitedThreadClosesItsHandle) {
  DetachThread(CreateExitedThread());

  const int handle_count = GetHandleCount();
  DetachThread(CreateExitedThread());
  EXPECT_EQ(GetHandleCount(), handle_count);
  EXPECT_EQ(GetActiveThreadCount(), 0);
}

}  // namespace
}  // namespace gb
