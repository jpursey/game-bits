// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "absl/log/log.h"
#include "gb/thread/fiber.h"

namespace gb {

bool SupportsFibers() { return false; }

void SetFiberVerboseLogging(bool) {}

std::vector<FiberThread> CreateFiberThreads(int, FiberOptions, uint32_t, void*,
                                            FiberMain) {
  LOG(ERROR) << "Job fibers not supported on this platform";
  return {};
}

Fiber CreateFiber(FiberOptions, uint32_t, void*, FiberMain) {
  LOG(ERROR) << "Job fibers not supported on this platform";
  return nullptr;
}

void DeleteFiber(Fiber) {
  LOG(ERROR) << "Job fibers not supported on this platform";
}

bool SwitchToFiber(Fiber) {
  LOG(ERROR) << "Job fibers not supported on this platform";
  return false;
}

std::string_view GetFiberName(Fiber) { return "null"; }

void SetFiberName(Fiber, std::string_view) {}

void* GetFiberData(Fiber) { return nullptr; }

void SetFiberData(Fiber, void*) {}

void* SwapFiberData(Fiber, void*) { return nullptr; }

Fiber GetThisFiber() { return nullptr; }

bool IsFiberRunning(Fiber) { return false; }

int GetRunningFiberCount() { return 0; }

}  // namespace gb
