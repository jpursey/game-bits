// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/cpu_topology.h"

#include <cstdint>

#include "absl/container/flat_hash_set.h"
#include "gb/thread/cpu_topology_internal.h"
#include "gb/thread/thread.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// Sets the int at `user_data` to the class of the core the thread is on.
void ReadCoreClass(void* user_data) {
  *static_cast<int*>(user_data) = GetThisThreadCoreClass();
}

TEST(CpuTopologyTest, HasEveryHardwareThreadInOneCore) {
  const CpuTopology& topology = GetCpuTopology();
  ASSERT_FALSE(topology.cores.empty());

  int hardware_thread_count = 0;
  absl::flat_hash_set<uint64_t> hardware_threads;
  for (const CpuCore& core : topology.cores) {
    EXPECT_FALSE(core.hardware_threads.empty());
    for (uint64_t hardware_thread : core.hardware_threads) {
      ++hardware_thread_count;
      if (hardware_thread != 0) {
        EXPECT_TRUE(hardware_threads.insert(hardware_thread).second)
            << "Hardware thread " << hardware_thread << " is in two cores";
      }
    }
  }
  EXPECT_EQ(hardware_thread_count, GetMaxConcurrency());

  const auto affinities = GetHardwareThreadAffinities();
  EXPECT_EQ(hardware_threads.size(), affinities.size());
  for (uint64_t affinity : affinities) {
    EXPECT_TRUE(hardware_threads.contains(affinity))
        << "Hardware thread " << affinity << " is in no core";
  }
}

TEST(CpuTopologyTest, CoresHaveValidCaches) {
  const CpuTopology& topology = GetCpuTopology();
  const int cache_count = static_cast<int>(topology.caches.size());
  for (const CpuCore& core : topology.cores) {
    ASSERT_GE(core.l2_cache, -1);
    ASSERT_LT(core.l2_cache, cache_count);
    ASSERT_GE(core.last_level_cache, -1);
    ASSERT_LT(core.last_level_cache, cache_count);
    if (core.l2_cache >= 0) {
      EXPECT_EQ(topology.caches[core.l2_cache].level, 2);
      ASSERT_GE(core.last_level_cache, 0);
      EXPECT_GE(topology.caches[core.last_level_cache].level, 2);
    }
  }
}

TEST(CpuTopologyTest, HardwareThreadTopologyHasACorePerHardwareThread) {
  const CpuTopology topology = internal::MakeHardwareThreadTopology();
  EXPECT_TRUE(topology.caches.empty());
  ASSERT_EQ(topology.cores.size(), GetMaxConcurrency());

  const auto affinities = GetHardwareThreadAffinities();
  for (int i = 0; i < static_cast<int>(topology.cores.size()); ++i) {
    SCOPED_TRACE(i);
    const CpuCore& core = topology.cores[i];
    const uint64_t affinity = (affinities.empty() ? 0 : affinities[i]);
    EXPECT_EQ(core.core_class, 0);
    EXPECT_EQ(core.affinity, affinity);
    ASSERT_EQ(core.hardware_threads.size(), 1);
    EXPECT_EQ(core.hardware_threads[0], affinity);
    EXPECT_EQ(core.l2_cache, -1);
    EXPECT_EQ(core.last_level_cache, -1);
  }
}

TEST(CpuTopologyTest, ThreadOnEachCoreHasItsClass) {
  for (const CpuCore& core : GetCpuTopology().cores) {
    int core_class = -1;
    Thread thread = CreateThread(core.affinity, 0, &core_class, ReadCoreClass);
    ASSERT_NE(thread, nullptr);
    JoinThread(thread);
    EXPECT_EQ(core_class, core.core_class)
        << "Thread pinned to affinity " << core.affinity;
  }
}

}  // namespace
}  // namespace gb
