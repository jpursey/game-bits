// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/cpu_topology_internal.h"

#include <cstdint>

#include "gb/thread/thread.h"

namespace gb::internal {

CpuTopology MakeHardwareThreadTopology() {
  CpuTopology topology;
  const auto affinities = GetHardwareThreadAffinities();
  if (!affinities.empty()) {
    for (uint64_t affinity : affinities) {
      CpuCore& core = topology.cores.emplace_back();
      core.affinity = affinity;
      core.hardware_threads.push_back(affinity);
    }
    return topology;
  }
  topology.cores.resize(GetMaxConcurrency());
  for (CpuCore& core : topology.cores) {
    core.hardware_threads.push_back(0);
  }
  return topology;
}

}  // namespace gb::internal
