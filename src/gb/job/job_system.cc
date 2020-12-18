// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/job/job_system.h"

namespace gb {

namespace {

thread_local JobSystem* tls_job_system = nullptr;

}  // namespace

JobSystem* JobSystem::Get() { return tls_job_system; }

void JobSystem::SetThreadState() { tls_job_system = this; }

}  // namespace gb
