// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_LOCAL_FILE_TIME_H_
#define GB_FILE_LOCAL_FILE_TIME_H_

#include <filesystem>

#include "absl/time/time.h"

namespace gb::internal {

// Converts between std::filesystem file times and absl::Time. File times keep
// the precision of the standard library's file clock, so a time converted to
// one is rounded down to it.

absl::Time FromLocalFileTime(std::filesystem::file_time_type time);
std::filesystem::file_time_type ToLocalFileTime(absl::Time time);

}  // namespace gb::internal

#endif  // GB_FILE_LOCAL_FILE_TIME_H_
