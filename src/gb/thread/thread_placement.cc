// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/thread_placement.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"

namespace gb {

namespace {

// Returns the indices of the cores, fastest class first, and in processor order
// within a class.
std::vector<int> OrderCoresFastestFirst(const CpuTopology* topology) {
  std::vector<int> order(topology->cores.size());
  std::iota(order.begin(), order.end(), 0);
  absl::c_stable_sort(order, [topology](int a, int b) {
    return topology->cores[a].core_class > topology->cores[b].core_class;
  });
  return order;
}

// Returns a place for a thread on each core in `cores`, pinned to all of its
// hardware threads.
std::vector<PlacedThread> GetCorePlaces(const CpuTopology* topology,
                                        absl::Span<const int> cores) {
  std::vector<PlacedThread> places;
  for (int core : cores) {
    places.push_back(
        {.affinity = topology->cores[core].affinity, .core = core});
  }
  return places;
}

// Returns a place for a thread on each hardware thread of the cores in
// `cores`, with the first hardware thread of every core before the second of
// any.
std::vector<PlacedThread> GetHardwareThreadPlaces(const CpuTopology* topology,
                                                  absl::Span<const int> cores) {
  int max_threads_per_core = 0;
  for (int core : cores) {
    max_threads_per_core = std::max(
        max_threads_per_core,
        static_cast<int>(topology->cores[core].hardware_threads.size()));
  }
  std::vector<PlacedThread> places;
  for (int thread = 0; thread < max_threads_per_core; ++thread) {
    for (int core : cores) {
      const std::vector<uint64_t>& hardware_threads =
          topology->cores[core].hardware_threads;
      if (thread < static_cast<int>(hardware_threads.size())) {
        places.push_back({.affinity = hardware_threads[thread], .core = core});
      }
    }
  }
  return places;
}

// Appends each run of consecutive cores with the same class and count, such as
// "cores 0-7 (class 1, 2 threads each)", leaving out cores with a count of
// zero.
void AppendCores(const CpuTopology* topology, absl::Span<const int> counts,
                 std::string* text) {
  const int core_count = static_cast<int>(topology->cores.size());
  std::string_view separator;
  for (int start = 0; start < core_count;) {
    const int count = counts[start];
    const int core_class = topology->cores[start].core_class;
    int end = start + 1;
    while (end < core_count && counts[end] == count &&
           topology->cores[end].core_class == core_class) {
      ++end;
    }
    if (count > 0) {
      absl::StrAppend(text, separator);
      if (end - start == 1) {
        absl::StrAppend(text, "core ", start);
      } else {
        absl::StrAppend(text, "cores ", start, "-", end - 1);
      }
      absl::StrAppend(text, " (class ", core_class);
      if (count > 1) {
        absl::StrAppend(text, ", ", count, " threads each");
      }
      absl::StrAppend(text, ")");
      separator = ", ";
    }
    start = end;
  }
}

}  // namespace

ThreadPlacement PlaceThreads(const ThreadPlacementOptions& options,
                             const CpuTopology* topology) {
  if (topology == nullptr) {
    topology = &GetCpuTopology();
  }
  ThreadPlacement placement;
  const std::vector<int> order = OrderCoresFastestFirst(topology);
  const int core_count = static_cast<int>(order.size());
  const int reserved_count =
      std::clamp(options.reserved_cores, 0, std::max(core_count - 1, 0));
  placement.reserved_cores.assign(order.begin(),
                                  order.begin() + reserved_count);
  const auto cores = absl::MakeConstSpan(order).subspan(reserved_count);

  std::vector<PlacedThread> places =
      options.pinning == ThreadPinning::kCore
          ? GetCorePlaces(topology, cores)
          : GetHardwareThreadPlaces(topology, cores);
  const int place_count = static_cast<int>(places.size());
  const int thread_count =
      (options.thread_count > 0 ? options.thread_count
                                : std::max(place_count, 1));
  if (options.pinning == ThreadPinning::kNone || thread_count > place_count) {
    placement.threads.resize(thread_count);
    return placement;
  }
  places.resize(thread_count);

  // A topology that can't pin threads has affinities of zero.
  for (PlacedThread& thread : places) {
    if (thread.affinity == 0) {
      thread.core = -1;
    }
  }
  placement.threads = std::move(places);
  return placement;
}

std::string DescribeThreadPlacement(const ThreadPlacement& placement,
                                    const CpuTopology* topology) {
  if (topology == nullptr) {
    topology = &GetCpuTopology();
  }
  const int core_count = static_cast<int>(topology->cores.size());
  std::vector<int> thread_counts(core_count, 0);
  bool pinned = false;
  for (const PlacedThread& thread : placement.threads) {
    if (thread.core >= 0 && thread.core < core_count) {
      ++thread_counts[thread.core];
      pinned = true;
    }
  }
  std::vector<int> reserved(core_count, 0);
  for (int core : placement.reserved_cores) {
    if (core >= 0 && core < core_count) {
      reserved[core] = 1;
    }
  }

  std::string text = absl::StrCat(placement.threads.size(), " threads");
  if (pinned) {
    absl::StrAppend(&text, ": ");
    AppendCores(topology, thread_counts, &text);
  } else {
    absl::StrAppend(&text, ", not pinned");
  }
  if (!placement.reserved_cores.empty()) {
    absl::StrAppend(&text, "; reserved: ");
    AppendCores(topology, reserved, &text);
  }
  return text;
}

}  // namespace gb
