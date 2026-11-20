// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <cinttypes>
#include <cstdio>

#include "gb/base/process_memory.h"

namespace gb {

std::optional<int64_t> GetProcessMemoryBytes() {
  std::FILE* file = std::fopen("/proc/self/status", "r");
  if (file == nullptr) {
    return std::nullopt;
  }

  // Lines look like "RssAnon:     1234 kB". A missing VmSwap counts as no
  // swap.
  bool has_resident = false;
  int64_t total_kb = 0;
  char line[256];
  while (std::fgets(line, sizeof(line), file) != nullptr) {
    int64_t kb = 0;
    if (std::sscanf(line, "RssAnon: %" SCNd64, &kb) == 1) {
      has_resident = true;
      total_kb += kb;
    } else if (std::sscanf(line, "VmSwap: %" SCNd64, &kb) == 1) {
      total_kb += kb;
    }
  }
  std::fclose(file);
  if (!has_resident) {
    return std::nullopt;
  }
  return total_kb * 1024;
}

}  // namespace gb
