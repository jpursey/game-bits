// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/fiber.h"
#include "glog/logging.h"

namespace gb {

bool SupportsFibers() { return false; }

void SetFiberVerboseLogging(bool) {}

int GetMaxConcurrency() { return 1; }

std::vector<Fiber> CreateFiberThreads(int thread_count, bool pin_threads,
                                      size_t stack_size, void* user_data,
                                      FiberMain fiber_main) {
  LOG(ERROR) << "Job fibers not supported on this platform";
  return {};
}

Fiber CreateFiber(size_t stack_size, void* user_data, FiberMain fiber_main) {
  LOG(ERROR) << "Job fibers not supported on this platform";
  return nullptr;
}

void DeleteFiber(Fiber fiber) {
  LOG(ERROR) << "Job fibers not supported on this platform";
}

bool SwitchToFiber(Fiber fiber) {
  LOG(ERROR) << "Job fibers not supported on this platform";
  return false;
}

Fiber GetThisFiber() { return nullptr; }

bool IsFiberRunning(Fiber fiber) { return false; }

}  // namespace gb
