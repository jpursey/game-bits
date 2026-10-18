// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "absl/base/no_destructor.h"
#include "gb/thread/cpu_topology.h"
#include "gb/thread/cpu_topology_internal.h"
#include "gb/thread/thread.h"

namespace gb {

const CpuTopology& GetCpuTopology() {
  static const absl::NoDestructor<CpuTopology> s_topology(
      internal::MakeHardwareThreadTopology());
  return *s_topology;
}

int GetThisThreadCoreClass() { return 0; }

}  // namespace gb
