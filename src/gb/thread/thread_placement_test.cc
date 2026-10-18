// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/thread_placement.h"

#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "gb/thread/cpu_topology.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

uint64_t Bit(int processor) { return uint64_t{1} << processor; }

// Returns the integers from `first` up to but not including `end`.
std::vector<int> Range(int first, int end) {
  std::vector<int> range(end - first);
  std::iota(range.begin(), range.end(), first);
  return range;
}

// Adds `count` cores of class `core_class`, with `threads_per_core` hardware
// threads each, numbered on from `*processor`.
void AddCores(int count, int core_class, int threads_per_core, int* processor,
              CpuTopology* topology) {
  for (int i = 0; i < count; ++i) {
    CpuCore& core = topology->cores.emplace_back();
    core.core_class = core_class;
    for (int thread = 0; thread < threads_per_core; ++thread) {
      const uint64_t hardware_thread = Bit((*processor)++);
      core.hardware_threads.push_back(hardware_thread);
      core.affinity |= hardware_thread;
    }
  }
}

// Returns a topology like an i9-14900KF's: 8 cores of class 1 with two
// hardware threads each (processors 0-15), and 16 cores of class 0 with one
// each (processors 16-31).
CpuTopology MakeHybridTopology() {
  CpuTopology topology;
  int processor = 0;
  AddCores(8, 1, 2, &processor, &topology);
  AddCores(16, 0, 1, &processor, &topology);
  return topology;
}

// Returns the core of each hardware thread of MakeHybridTopology() from core
// `first_core` on, in the order PlaceThreads() takes them: the first hardware
// thread of every core, then the second hardware thread of each class 1 core.
std::vector<int> GetHybridCoreOrder(int first_core) {
  std::vector<int> cores = Range(first_core, 24);
  const std::vector<int> second_threads = Range(first_core, 8);
  cores.insert(cores.end(), second_threads.begin(), second_threads.end());
  return cores;
}

CpuTopology MakeUniformTopology(int core_count, int threads_per_core) {
  CpuTopology topology;
  int processor = 0;
  AddCores(core_count, 0, threads_per_core, &processor, &topology);
  return topology;
}

// Returns a topology like the one on a platform that cannot pin threads.
CpuTopology MakeUnpinnableTopology(int core_count) {
  CpuTopology topology;
  topology.cores.resize(core_count);
  for (CpuCore& core : topology.cores) {
    core.hardware_threads.push_back(0);
  }
  return topology;
}

ThreadPlacement Place(const CpuTopology& topology,
                      const ThreadPlacementOptions& options) {
  return PlaceThreads(options, &topology);
}

std::string Describe(const CpuTopology& topology,
                     const ThreadPlacement& placement) {
  return DescribeThreadPlacement(placement, &topology);
}

std::vector<uint64_t> GetAffinities(const ThreadPlacement& placement) {
  std::vector<uint64_t> affinities;
  for (const PlacedThread& thread : placement.threads) {
    affinities.push_back(thread.affinity);
  }
  return affinities;
}

std::vector<int> GetCores(const ThreadPlacement& placement) {
  std::vector<int> cores;
  for (const PlacedThread& thread : placement.threads) {
    cores.push_back(thread.core);
  }
  return cores;
}

void ExpectNotPinned(const ThreadPlacement& placement, int thread_count) {
  EXPECT_EQ(placement.threads.size(), thread_count);
  EXPECT_THAT(GetAffinities(placement), Each(uint64_t{0}));
  EXPECT_THAT(GetCores(placement), Each(-1));
}

TEST(ThreadPlacementTest, DefaultPlacesEveryHardwareThreadFastestCoresFirst) {
  const ThreadPlacement placement = Place(MakeHybridTopology(), {});

  ASSERT_EQ(placement.threads.size(), 32);
  EXPECT_THAT(GetCores(placement), ElementsAreArray(GetHybridCoreOrder(0)));
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(placement.threads[i].affinity, Bit(2 * i)) << i;
    EXPECT_EQ(placement.threads[24 + i].affinity, Bit(2 * i + 1)) << 24 + i;
  }
  for (int i = 8; i < 24; ++i) {
    EXPECT_EQ(placement.threads[i].affinity, Bit(8 + i)) << i;
  }
  EXPECT_THAT(placement.reserved_cores, IsEmpty());
}

TEST(ThreadPlacementTest, FewerThreadsSpreadAcrossCores) {
  const CpuTopology topology = MakeHybridTopology();
  const ThreadPlacement placement = Place(topology, {.thread_count = 24});

  EXPECT_THAT(GetCores(placement), ElementsAreArray(Range(0, 24)));
  const std::vector<uint64_t> every_affinity =
      GetAffinities(Place(topology, {}));
  EXPECT_THAT(GetAffinities(placement),
              ElementsAreArray(absl::MakeConstSpan(every_affinity).first(24)));
}

TEST(ThreadPlacementTest, MoreThreadsThanHardwareThreadsAreNotPinned) {
  const ThreadPlacement placement =
      Place(MakeHybridTopology(), {.thread_count = 33});

  ExpectNotPinned(placement, 33);
}

TEST(ThreadPlacementTest, CorePinningPinsEachThreadToAWholeCore) {
  const CpuTopology topology = MakeHybridTopology();
  const ThreadPlacement placement =
      Place(topology, {.pinning = ThreadPinning::kCore});

  ASSERT_EQ(placement.threads.size(), 24);
  EXPECT_THAT(GetCores(placement), ElementsAreArray(Range(0, 24)));
  for (int i = 0; i < 24; ++i) {
    EXPECT_EQ(placement.threads[i].affinity, topology.cores[i].affinity) << i;
  }
}

TEST(ThreadPlacementTest, CorePinningWithFewerThreadsTakesTheFastestCores) {
  const ThreadPlacement placement =
      Place(MakeHybridTopology(),
            {.pinning = ThreadPinning::kCore, .thread_count = 4});

  EXPECT_THAT(GetCores(placement), ElementsAre(0, 1, 2, 3));
}

TEST(ThreadPlacementTest, CorePinningWithMoreThreadsThanCoresIsNotPinned) {
  const ThreadPlacement placement =
      Place(MakeHybridTopology(),
            {.pinning = ThreadPinning::kCore, .thread_count = 25});

  ExpectNotPinned(placement, 25);
}

TEST(ThreadPlacementTest, ReservedCoresAreTheFastest) {
  const CpuTopology topology = MakeHybridTopology();

  const ThreadPlacement cores =
      Place(topology, {.pinning = ThreadPinning::kCore, .reserved_cores = 1});
  EXPECT_THAT(GetCores(cores), ElementsAreArray(Range(1, 24)));
  EXPECT_THAT(cores.reserved_cores, ElementsAre(0));

  const ThreadPlacement hardware_threads =
      Place(topology, {.reserved_cores = 2});
  EXPECT_THAT(GetCores(hardware_threads),
              ElementsAreArray(GetHybridCoreOrder(2)));
  EXPECT_THAT(hardware_threads.reserved_cores, UnorderedElementsAre(0, 1));
}

TEST(ThreadPlacementTest, ReservedCoresLeaveAtLeastOneCore) {
  const ThreadPlacement placement =
      Place(MakeUniformTopology(4, 2), {.reserved_cores = 10});

  EXPECT_THAT(placement.reserved_cores, UnorderedElementsAre(0, 1, 2));
  EXPECT_THAT(GetCores(placement), ElementsAre(3, 3));
  EXPECT_THAT(GetAffinities(placement), ElementsAre(Bit(6), Bit(7)));
}

TEST(ThreadPlacementTest, NegativeReservedCoresReserveNone) {
  const ThreadPlacement placement =
      Place(MakeUniformTopology(4, 1), {.reserved_cores = -1});

  EXPECT_THAT(placement.reserved_cores, IsEmpty());
  EXPECT_THAT(GetCores(placement), ElementsAre(0, 1, 2, 3));
}

TEST(ThreadPlacementTest, CoresOfAClassStayInProcessorOrder) {
  CpuTopology topology;
  int processor = 0;
  AddCores(1, 0, 1, &processor, &topology);
  AddCores(1, 1, 1, &processor, &topology);
  AddCores(1, 0, 1, &processor, &topology);
  AddCores(1, 1, 1, &processor, &topology);
  const ThreadPlacement placement =
      Place(topology, {.pinning = ThreadPinning::kCore});

  EXPECT_THAT(GetCores(placement), ElementsAre(1, 3, 0, 2));
  EXPECT_THAT(GetAffinities(placement),
              ElementsAre(Bit(1), Bit(3), Bit(0), Bit(2)));
}

TEST(ThreadPlacementTest, ReservedCoresAreTheFastestClassFirst) {
  CpuTopology topology;
  int processor = 0;
  AddCores(2, 1, 1, &processor, &topology);
  AddCores(1, 2, 1, &processor, &topology);
  AddCores(1, 0, 1, &processor, &topology);
  const ThreadPlacement placement =
      Place(topology, {.pinning = ThreadPinning::kCore, .reserved_cores = 2});

  EXPECT_THAT(placement.reserved_cores, UnorderedElementsAre(0, 2));
  EXPECT_THAT(GetCores(placement), ElementsAre(1, 3));
}

TEST(ThreadPlacementTest, NoPinningCountsHardwareThreadsThatArentReserved) {
  const ThreadPlacement placement =
      Place(MakeHybridTopology(),
            {.pinning = ThreadPinning::kNone, .reserved_cores = 1});

  ExpectNotPinned(placement, 30);
  EXPECT_THAT(placement.reserved_cores, ElementsAre(0));
}

TEST(ThreadPlacementTest, TopologyThatCannotPinIsNotPinned) {
  const ThreadPlacement placement = Place(MakeUnpinnableTopology(4), {});

  ExpectNotPinned(placement, 4);
}

TEST(ThreadPlacementTest, DescribesHardwareThreads) {
  const CpuTopology topology = MakeHybridTopology();

  EXPECT_EQ(Describe(topology, Place(topology, {})),
            "32 threads: cores 0-7 (class 1, 2 threads each), cores 8-23 "
            "(class 0)");
  EXPECT_EQ(Describe(topology, Place(topology, {.thread_count = 26})),
            "26 threads: cores 0-1 (class 1, 2 threads each), cores 2-7 "
            "(class 1), cores 8-23 (class 0)");
}

TEST(ThreadPlacementTest, DescribesReservedCores) {
  const CpuTopology topology = MakeHybridTopology();

  EXPECT_EQ(Describe(topology, Place(topology, {.pinning = ThreadPinning::kCore,
                                                .reserved_cores = 1})),
            "23 threads: cores 1-7 (class 1), cores 8-23 (class 0); "
            "reserved: core 0 (class 1)");
}

TEST(ThreadPlacementTest, DescribesThreadsThatAreNotPinned) {
  const CpuTopology topology = MakeHybridTopology();

  EXPECT_EQ(Describe(topology, Place(topology, {.pinning = ThreadPinning::kNone,
                                                .reserved_cores = 1})),
            "30 threads, not pinned; reserved: core 0 (class 1)");
  const CpuTopology unpinnable = MakeUnpinnableTopology(4);
  EXPECT_EQ(Describe(unpinnable, Place(unpinnable, {})),
            "4 threads, not pinned");
}

TEST(ThreadPlacementTest, NoTopologyUsesTheProcessTopology) {
  const ThreadPlacementOptions options = {.pinning = ThreadPinning::kCore};
  const ThreadPlacement placement = PlaceThreads(options);
  const ThreadPlacement process_placement =
      PlaceThreads(options, &GetCpuTopology());

  EXPECT_EQ(GetAffinities(placement), GetAffinities(process_placement));
  EXPECT_EQ(GetCores(placement), GetCores(process_placement));
  EXPECT_EQ(DescribeThreadPlacement(placement),
            DescribeThreadPlacement(placement, &GetCpuTopology()));
}

}  // namespace
}  // namespace gb
