// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/win_cpu_topology.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/algorithm/container.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb {
namespace {

using ::testing::ElementsAre;

// The sizes Windows writes for records with one group mask.
constexpr DWORD kProcessorRecordSize = 48;
constexpr DWORD kCacheRecordSize = 56;

constexpr DWORD kKiB = 1024;
constexpr DWORD kMiB = 1024 * kKiB;

// Level 2 (L2) and level 3 (L3) cache sizes of an i9-14900KF.
constexpr DWORD kPerformanceL2SizeBytes = 2 * kMiB;
constexpr DWORD kEfficiencyL2SizeBytes = 4 * kMiB;
constexpr DWORD kL3SizeBytes = 36 * kMiB;

// Returns a record for a core, module, or package.
SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX MakeProcessor(
    LOGICAL_PROCESSOR_RELATIONSHIP relationship, int efficiency_class,
    int group, uint64_t mask) {
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = {};
  info.Relationship = relationship;
  info.Size = kProcessorRecordSize;
  info.Processor.EfficiencyClass = static_cast<BYTE>(efficiency_class);
  info.Processor.GroupCount = 1;
  info.Processor.GroupMask[0].Group = static_cast<WORD>(group);
  info.Processor.GroupMask[0].Mask = mask;
  return info;
}

SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX MakeCache(int level,
                                                  PROCESSOR_CACHE_TYPE type,
                                                  DWORD size_bytes, int group,
                                                  uint64_t mask) {
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = {};
  info.Relationship = RelationCache;
  info.Size = kCacheRecordSize;
  info.Cache.Level = static_cast<BYTE>(level);
  info.Cache.CacheSize = size_bytes;
  info.Cache.Type = type;
  info.Cache.GroupCount = 1;
  info.Cache.GroupMask.Group = static_cast<WORD>(group);
  info.Cache.GroupMask.Mask = mask;
  return info;
}

// Appends the first `size_bytes` of a record. Windows writes as many as the
// record's Size, which may be fewer than the size of the structure.
void AddRecordBytes(const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX& info,
                    DWORD size_bytes, std::vector<std::byte>* buffer) {
  const auto* bytes = reinterpret_cast<const std::byte*>(&info);
  buffer->insert(buffer->end(), bytes, bytes + size_bytes);
}

void AddRecord(const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX& info,
               std::vector<std::byte>* buffer) {
  AddRecordBytes(info, info.Size, buffer);
}

// Adds a core with its own level 1 caches and L2 cache.
void AddCoreWithL2(int efficiency_class, int group, uint64_t mask,
                   DWORD l2_size_bytes, std::vector<std::byte>* buffer) {
  AddRecord(MakeProcessor(RelationProcessorCore, efficiency_class, group, mask),
            buffer);
  AddRecord(MakeCache(1, CacheInstruction, 48 * kKiB, group, mask), buffer);
  AddRecord(MakeCache(1, CacheData, 32 * kKiB, group, mask), buffer);
  AddRecord(MakeCache(2, CacheUnified, l2_size_bytes, group, mask), buffer);
}

// Returns a buffer like the one an i9-14900KF has, in the order Windows writes
// it: 8 performance cores of class 1 with two hardware threads each
// (processors 0-15) and an L2 cache each, and 16 efficiency cores of class 0
// (processors 16-31) sharing an L2 cache in clusters of four, all sharing one
// L3 cache. The package and module records are skipped.
std::vector<std::byte> MakeHybridBuffer() {
  std::vector<std::byte> buffer;
  AddRecord(MakeProcessor(RelationProcessorPackage, 0, 0, 0xffffffff), &buffer);
  for (int i = 0; i < 8; ++i) {
    const uint64_t mask = uint64_t{3} << (2 * i);
    AddRecord(MakeProcessor(RelationProcessorModule, 0, 0, mask), &buffer);
    AddCoreWithL2(1, 0, mask, kPerformanceL2SizeBytes, &buffer);
    if (i == 0) {
      AddRecord(MakeCache(3, CacheUnified, kL3SizeBytes, 0, 0xffffffff),
                &buffer);
    }
  }
  for (int i = 0; i < 16; ++i) {
    const int processor = 16 + i;
    const uint64_t mask = uint64_t{1} << processor;
    AddRecord(MakeProcessor(RelationProcessorCore, 0, 0, mask), &buffer);
    AddRecord(MakeCache(1, CacheInstruction, 64 * kKiB, 0, mask), &buffer);
    AddRecord(MakeCache(1, CacheData, 32 * kKiB, 0, mask), &buffer);
    if (i % 4 == 0) {
      const uint64_t cluster_mask = uint64_t{0xf} << processor;
      AddRecord(MakeProcessor(RelationProcessorModule, 0, 0, cluster_mask),
                &buffer);
      AddRecord(
          MakeCache(2, CacheUnified, kEfficiencyL2SizeBytes, 0, cluster_mask),
          &buffer);
    }
  }
  return buffer;
}

// Returns a buffer for `core_count` cores of class 0 in one processor group,
// each with `threads_per_core` hardware threads and its own L2 cache, all
// sharing one L3 cache of `l3_size_bytes` (none if zero).
std::vector<std::byte> MakeUniformBuffer(int group, int core_count,
                                         int threads_per_core,
                                         DWORD l3_size_bytes) {
  std::vector<std::byte> buffer;
  const uint64_t core_mask = (uint64_t{1} << threads_per_core) - 1;
  const uint64_t all_mask =
      (uint64_t{1} << (core_count * threads_per_core)) - 1;
  for (int i = 0; i < core_count; ++i) {
    AddCoreWithL2(0, group, core_mask << (i * threads_per_core), 1 * kMiB,
                  &buffer);
    if (i == 0 && l3_size_bytes != 0) {
      AddRecord(MakeCache(3, CacheUnified, l3_size_bytes, group, all_mask),
                &buffer);
    }
  }
  return buffer;
}

int CountCaches(const CpuTopology& topology, int level) {
  return static_cast<int>(absl::c_count_if(
      topology.caches,
      [level](const CpuCache& cache) { return cache.level == level; }));
}

// Expects `count` cores from `first` to be of class `core_class`, with
// `threads_per_core` hardware threads each in processor order from
// `first_processor`, and an L2 cache that no other core shares.
void ExpectCoresWithOwnL2(const CpuTopology& topology, int first, int count,
                          int core_class, int threads_per_core,
                          int first_processor) {
  const uint64_t core_mask = (uint64_t{1} << threads_per_core) - 1;
  const int core_count = static_cast<int>(topology.cores.size());
  for (int i = first; i < first + count; ++i) {
    SCOPED_TRACE(i);
    const CpuCore& core = topology.cores[i];
    const int processor = first_processor + (i - first) * threads_per_core;
    EXPECT_EQ(core.core_class, core_class);
    EXPECT_EQ(core.affinity, core_mask << processor);
    ASSERT_EQ(core.hardware_threads.size(), threads_per_core);
    for (int thread = 0; thread < threads_per_core; ++thread) {
      EXPECT_EQ(core.hardware_threads[thread],
                uint64_t{1} << (processor + thread));
    }
    ASSERT_GE(core.l2_cache, 0);
    EXPECT_EQ(topology.caches[core.l2_cache].level, 2);
    for (int other = 0; other < core_count; ++other) {
      if (other != i) {
        EXPECT_NE(topology.cores[other].l2_cache, core.l2_cache) << other;
      }
    }
  }
}

TEST(WinCpuTopologyTest, HybridCpu) {
  const CpuTopology topology =
      internal::ParseCpuTopology(MakeHybridBuffer(), 0, 0xffffffff);

  ASSERT_EQ(topology.cores.size(), 24);
  EXPECT_EQ(topology.caches.size(), 13);
  EXPECT_EQ(CountCaches(topology, 2), 12);
  const int l3 = topology.cores[0].last_level_cache;
  ASSERT_GE(l3, 0);
  EXPECT_EQ(topology.caches[l3].level, 3);
  EXPECT_EQ(topology.caches[l3].size_bytes, kL3SizeBytes);

  // Performance cores.
  ExpectCoresWithOwnL2(topology, 0, 8, 1, 2, 0);
  for (int i = 0; i < 8; ++i) {
    SCOPED_TRACE(i);
    const CpuCore& core = topology.cores[i];
    ASSERT_GE(core.l2_cache, 0);
    EXPECT_EQ(topology.caches[core.l2_cache].size_bytes,
              kPerformanceL2SizeBytes);
    EXPECT_EQ(core.last_level_cache, l3);
  }

  // Efficiency cores, sharing an L2 cache in clusters of four.
  for (int i = 8; i < 24; ++i) {
    SCOPED_TRACE(i);
    const CpuCore& core = topology.cores[i];
    const int processor = 8 + i;
    EXPECT_EQ(core.core_class, 0);
    EXPECT_EQ(core.affinity, uint64_t{1} << processor);
    EXPECT_THAT(core.hardware_threads, ElementsAre(uint64_t{1} << processor));
    ASSERT_GE(core.l2_cache, 0);
    EXPECT_EQ(topology.caches[core.l2_cache].size_bytes,
              kEfficiencyL2SizeBytes);
    const int cluster_start = i - (i - 8) % 4;
    EXPECT_EQ(core.l2_cache, topology.cores[cluster_start].l2_cache);
    if (cluster_start > 8) {
      EXPECT_NE(core.l2_cache, topology.cores[cluster_start - 1].l2_cache);
    }
    EXPECT_EQ(core.last_level_cache, l3);
  }
}

TEST(WinCpuTopologyTest, UniformCpuWithTwoThreadsPerCore) {
  const CpuTopology topology = internal::ParseCpuTopology(
      MakeUniformBuffer(0, 8, 2, 32 * kMiB), 0, 0xffff);

  ASSERT_EQ(topology.cores.size(), 8);
  EXPECT_EQ(topology.caches.size(), 9);
  ExpectCoresWithOwnL2(topology, 0, 8, 0, 2, 0);
  const int l3 = topology.cores[0].last_level_cache;
  ASSERT_GE(l3, 0);
  EXPECT_EQ(topology.caches[l3].level, 3);
  for (const CpuCore& core : topology.cores) {
    EXPECT_EQ(core.last_level_cache, l3);
  }
}

TEST(WinCpuTopologyTest, UniformCpuWithOneThreadPerCoreAndNoL3) {
  const CpuTopology topology =
      internal::ParseCpuTopology(MakeUniformBuffer(0, 4, 1, 0), 0, 0xf);

  ASSERT_EQ(topology.cores.size(), 4);
  EXPECT_EQ(topology.caches.size(), 4);
  ExpectCoresWithOwnL2(topology, 0, 4, 0, 1, 0);
  for (const CpuCore& core : topology.cores) {
    EXPECT_EQ(core.last_level_cache, core.l2_cache);
  }
}

TEST(WinCpuTopologyTest, TwoProcessorGroups) {
  std::vector<std::byte> buffer = MakeUniformBuffer(0, 4, 2, 32 * kMiB);
  const std::vector<std::byte> group_1 = MakeUniformBuffer(1, 4, 2, 16 * kMiB);
  buffer.insert(buffer.end(), group_1.begin(), group_1.end());

  for (int group = 0; group < 2; ++group) {
    SCOPED_TRACE(group);
    const CpuTopology topology =
        internal::ParseCpuTopology(buffer, group, 0xff);
    ASSERT_EQ(topology.cores.size(), 4);
    EXPECT_EQ(topology.caches.size(), 5);
    ExpectCoresWithOwnL2(topology, 0, 4, 0, 2, 0);
    for (const CpuCore& core : topology.cores) {
      ASSERT_GE(core.last_level_cache, 0);
      EXPECT_EQ(topology.caches[core.last_level_cache].size_bytes,
                group == 0 ? 32 * kMiB : 16 * kMiB);
    }
  }
}

TEST(WinCpuTopologyTest, ProcessMaskLeavesOutHardwareThreads) {
  // Leaves out the second hardware thread of the first core, all of the second
  // core, and the first cluster of efficiency cores.
  const uint64_t mask = 0xffffffff & ~uint64_t{0b1110} & ~(uint64_t{0xf} << 16);
  const CpuTopology topology =
      internal::ParseCpuTopology(MakeHybridBuffer(), 0, mask);

  ASSERT_EQ(topology.cores.size(), 19);
  EXPECT_EQ(topology.cores[0].affinity, 1);
  EXPECT_THAT(topology.cores[0].hardware_threads, ElementsAre(uint64_t{1}));
  EXPECT_EQ(topology.cores[1].affinity, uint64_t{3} << 4);
  EXPECT_EQ(topology.cores[7].affinity, uint64_t{1} << 20);
  EXPECT_EQ(CountCaches(topology, 2), 10);
  EXPECT_EQ(CountCaches(topology, 3), 1);
}

TEST(WinCpuTopologyTest, CacheWithoutGroupCount) {
  // Windows 10 leaves a cache's group count zero.
  std::vector<std::byte> buffer;
  const uint64_t mask = 1;
  AddRecord(MakeProcessor(RelationProcessorCore, 0, 0, mask), &buffer);
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX cache =
      MakeCache(2, CacheUnified, 1 * kMiB, 0, mask);
  cache.Cache.GroupCount = 0;
  AddRecord(cache, &buffer);
  const CpuTopology topology = internal::ParseCpuTopology(buffer, 0, mask);

  ASSERT_EQ(topology.cores.size(), 1);
  ASSERT_EQ(topology.caches.size(), 1);
  EXPECT_EQ(topology.cores[0].l2_cache, 0);
  EXPECT_EQ(topology.caches[0].size_bytes, 1 * kMiB);
}

TEST(WinCpuTopologyTest, StopsAtRecordPastTheEnd) {
  std::vector<std::byte> buffer;
  AddRecord(MakeProcessor(RelationProcessorCore, 0, 0, 1), &buffer);
  AddRecord(MakeProcessor(RelationProcessorCore, 0, 0, 2), &buffer);
  buffer.pop_back();
  const CpuTopology topology = internal::ParseCpuTopology(buffer, 0, 0xf);

  ASSERT_EQ(topology.cores.size(), 1);
  EXPECT_EQ(topology.cores[0].affinity, 1);
}

TEST(WinCpuTopologyTest, StopsAtRecordTooSmallForItsSize) {
  std::vector<std::byte> buffer;
  AddRecord(MakeProcessor(RelationProcessorCore, 0, 0, 1), &buffer);
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX empty =
      MakeProcessor(RelationProcessorCore, 0, 0, 2);
  empty.Size = 0;
  AddRecordBytes(empty, kProcessorRecordSize, &buffer);
  AddRecord(MakeProcessor(RelationProcessorCore, 0, 0, 4), &buffer);
  const CpuTopology topology = internal::ParseCpuTopology(buffer, 0, 0xf);

  ASSERT_EQ(topology.cores.size(), 1);
  EXPECT_EQ(topology.cores[0].affinity, 1);
}

TEST(WinCpuTopologyTest, SkipsCoreWhoseGroupMasksDoNotFit) {
  std::vector<std::byte> buffer;
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX core =
      MakeProcessor(RelationProcessorCore, 0, 0, 1);
  core.Processor.GroupCount = 2;
  AddRecord(core, &buffer);
  AddRecord(MakeProcessor(RelationProcessorCore, 0, 0, 2), &buffer);
  const CpuTopology topology = internal::ParseCpuTopology(buffer, 0, 0xf);

  ASSERT_EQ(topology.cores.size(), 1);
  EXPECT_EQ(topology.cores[0].affinity, 2);
}

}  // namespace
}  // namespace gb
