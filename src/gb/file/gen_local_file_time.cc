// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <chrono>

#include "gb/file/local_file_time.h"

namespace gb::internal {

namespace {

using std::chrono::file_clock;
using std::chrono::system_clock;

}  // namespace

absl::Time FromLocalFileTime(std::filesystem::file_time_type time) {
  return absl::FromChrono(std::chrono::time_point_cast<system_clock::duration>(
      file_clock::to_sys(time)));
}

std::filesystem::file_time_type ToLocalFileTime(absl::Time time) {
  return std::chrono::time_point_cast<file_clock::duration>(
      file_clock::from_sys(absl::ToChronoTime(time)));
}

}  // namespace gb::internal
