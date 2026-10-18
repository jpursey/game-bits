// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/win_cpu_topology.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/log.h"
#include "absl/numeric/bits.h"
#include "gb/base/win_platform.h"
#include "gb/thread/cpu_topology_internal.h"
#include "gb/thread/thread.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb {

namespace {

// Windows numbers processors within groups of up to 64.
constexpr int kGroupSize = 64;

// The topology of the cores the process can use, with the class of each
// processor in the process's group.
struct ProcessTopology {
  int group = 0;
  CpuTopology topology;
  int processor_classes[kGroupSize] = {};
};

// Returns the processors in `group` among `count` group masks, or zero if the
// masks run past `record_end`, the end of the record they are in.
uint64_t GetGroupMask(const GROUP_AFFINITY* masks, int count, int group,
                      const void* record_end) {
  const void* masks_end = masks + count;
  if (masks_end > record_end) {
    return 0;
  }
  uint64_t mask = 0;
  for (int i = 0; i < count; ++i) {
    if (masks[i].Group == group) {
      mask |= masks[i].Mask;
    }
  }
  return mask;
}

void AddCore(const PROCESSOR_RELATIONSHIP& processor, const void* record_end,
             int group, uint64_t mask, CpuTopology* topology) {
  const uint64_t core_mask =
      GetGroupMask(processor.GroupMask, processor.GroupCount, group,
                   record_end) &
      mask;
  if (core_mask == 0) {
    return;
  }
  CpuCore& core = topology->cores.emplace_back();
  core.core_class = processor.EfficiencyClass;
  core.affinity = core_mask;
  for (uint64_t bits = core_mask; bits != 0; bits &= bits - 1) {
    core.hardware_threads.push_back(uint64_t{1} << absl::countr_zero(bits));
  }
}

void AddCache(const CACHE_RELATIONSHIP& cache, const void* record_end,
              int group, uint64_t mask, CpuTopology* topology,
              std::vector<uint64_t>* cache_masks) {
  if (cache.Level < 2 ||
      (cache.Type != CacheUnified && cache.Type != CacheData)) {
    return;
  }

  // Windows 10 leaves the group count zero, and has one mask.
  const int group_count = std::max<int>(cache.GroupCount, 1);
  const uint64_t cache_mask =
      GetGroupMask(cache.GroupMasks, group_count, group, record_end) & mask;
  if (cache_mask == 0) {
    return;
  }
  topology->caches.push_back(
      {.level = cache.Level, .size_bytes = cache.CacheSize});
  cache_masks->push_back(cache_mask);
}

// Returns the topology of the hardware threads in GetHardwareThreadAffinities()
// and sets `group` to their processor group, or returns no cores if Windows
// can't tell.
CpuTopology ReadCpuTopology(int* group) {
  uint64_t mask = 0;
  for (uint64_t affinity : GetHardwareThreadAffinities()) {
    mask |= affinity;
  }

  // A process with threads in more than one processor group fails this, as
  // there is room for only one group. It has no hardware thread affinities
  // either.
  USHORT group_count = 1;
  USHORT process_group = 0;
  if (mask == 0 || !GetProcessGroupAffinity(GetCurrentProcess(), &group_count,
                                            &process_group)) {
    return {};
  }

  DWORD size = 0;
  GetLogicalProcessorInformationEx(RelationAll, nullptr, &size);
  std::vector<std::byte> buffer(size);
  if (!GetLogicalProcessorInformationEx(
          RelationAll,
          reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
              buffer.data()),
          &size)) {
    LOG(WARNING) << "Could not read the CPU topology: " << GetWindowsError();
    return {};
  }
  CpuTopology topology = internal::ParseCpuTopology(
      absl::MakeConstSpan(buffer.data(), size), process_group, mask);

  // Every hardware thread must be in a core, or placing threads by core would
  // leave some out.
  int hardware_thread_count = 0;
  for (const CpuCore& core : topology.cores) {
    hardware_thread_count += static_cast<int>(core.hardware_threads.size());
  }
  if (hardware_thread_count != absl::popcount(mask)) {
    LOG(WARNING) << "The CPU topology has " << hardware_thread_count
                 << " of the process's " << absl::popcount(mask)
                 << " hardware threads.";
    return {};
  }
  *group = process_group;
  return topology;
}

ProcessTopology ReadProcessTopology() {
  ProcessTopology process;
  process.topology = ReadCpuTopology(&process.group);
  if (process.topology.cores.empty()) {
    process.topology = internal::MakeHardwareThreadTopology();
  }
  for (const CpuCore& core : process.topology.cores) {
    for (uint64_t bits = core.affinity; bits != 0; bits &= bits - 1) {
      process.processor_classes[absl::countr_zero(bits)] = core.core_class;
    }
  }
  return process;
}

const ProcessTopology& GetProcessTopology() {
  static const absl::NoDestructor<ProcessTopology> s_topology(
      ReadProcessTopology());
  return *s_topology;
}

}  // namespace

namespace internal {

CpuTopology ParseCpuTopology(absl::Span<const std::byte> buffer, int group,
                             uint64_t mask) {
  CpuTopology topology;

  // The hardware threads that share each cache in topology.caches.
  std::vector<uint64_t> cache_masks;

  // Each record starts with its relationship and size. Parsing stops at a
  // record too small for that, or that runs past the end of the buffer.
  constexpr int kHeaderSize =
      offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor);
  const int buffer_size = static_cast<int>(buffer.size());
  int offset = 0;
  while (buffer_size - offset >= kHeaderSize) {
    const auto* info =
        reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
            buffer.data() + offset);
    const int info_size = static_cast<int>(info->Size);
    if (info_size < kHeaderSize || info_size > buffer_size - offset) {
      break;
    }
    const void* record_end = buffer.data() + offset + info_size;
    if (info->Relationship == RelationProcessorCore) {
      AddCore(info->Processor, record_end, group, mask, &topology);
    } else if (info->Relationship == RelationCache) {
      AddCache(info->Cache, record_end, group, mask, &topology, &cache_masks);
    }
    offset += info_size;
  }

  const int cache_count = static_cast<int>(topology.caches.size());
  for (CpuCore& core : topology.cores) {
    int last_level = 0;
    for (int i = 0; i < cache_count; ++i) {
      if ((cache_masks[i] & core.affinity) == 0) {
        continue;
      }
      const int level = topology.caches[i].level;
      if (level == 2) {
        core.l2_cache = i;
      }
      if (level > last_level) {
        core.last_level_cache = i;
        last_level = level;
      }
    }
  }
  return topology;
}

}  // namespace internal

const CpuTopology& GetCpuTopology() { return GetProcessTopology().topology; }

int GetThisThreadCoreClass() {
  const ProcessTopology& process = GetProcessTopology();
  PROCESSOR_NUMBER processor;
  GetCurrentProcessorNumberEx(&processor);
  if (processor.Group != process.group) {
    return 0;
  }
  return process.processor_classes[processor.Number];
}

}  // namespace gb
