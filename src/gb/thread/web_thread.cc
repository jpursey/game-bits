// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <cstdint>
#include <string_view>

#include "absl/log/log.h"
#include "absl/types/span.h"
#include "gb/thread/thread.h"

namespace gb {

//==============================================================================
// A web build without threads has only the program's own thread, which
// CreateThread() did not create, so there is never a Thread to act on.
//==============================================================================

void SetThreadVerboseLogging(bool) {}

int GetMaxConcurrency() { return 1; }

absl::Span<const uint64_t> GetHardwareThreadAffinities() { return {}; }

bool SupportsThreads() { return false; }

Thread CreateThread(uint64_t, uint32_t, void*, ThreadMain) {
  LOG(ERROR) << "Threads not supported in a web build without threads";
  return nullptr;
}

void JoinThread(Thread) {
  LOG(ERROR) << "JoinThread called, but there are no threads to join";
}

void DetachThread(Thread) {
  LOG(ERROR) << "DetachThread called, but there are no threads to detach";
}

uint64_t GetThreadAffinity(Thread) { return 0; }

bool SetThisThreadAffinity(uint64_t) { return false; }

std::string_view GetThreadName(Thread) { return "null"; }

void SetThreadName(Thread, std::string_view) {}

Thread GetThisThread() { return nullptr; }

ThreadCapabilities GetThreadCapabilities() { return {}; }

int64_t GetThisThreadCycles() { return 0; }

int GetActiveThreadCount() { return 0; }

}  // namespace gb
