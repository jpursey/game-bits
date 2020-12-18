// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/job_fiber.h"
#include "glog/logging.h"

namespace gb {

bool SupportsJobFibers() { return false; }

std::vector<JobFiber> CreateJobFiberThreads(int thread_count, bool pin_threads,
                                            size_t stack_size, void* user_data,
                                            JobFiberMain fiber_main) {
  LOG(ERROR) << "Job fibers not supported on this platform";
  return {};
}

JobFiber CreateJobFiber(size_t stack_size, void* user_data,
                        JobFiberMain fiber_main) {
  LOG(ERROR) << "Job fibers not supported on this platform";
  return nullptr;
}

void DeleteJobFiber(JobFiber fiber) {
  LOG(ERROR) << "Job fibers not supported on this platform";
}

bool SwitchToJobFiber(JobFiber fiber) {
  LOG(ERROR) << "Job fibers not supported on this platform";
  return false;
}

JobFiber GetThisJobFiber() { return nullptr; }

bool IsJobFiberRunning(JobFiber fiber) { return false; }

}  // namespace gb
