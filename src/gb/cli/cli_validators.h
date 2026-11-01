// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_VALIDATORS_H_
#define GB_CLI_CLI_VALIDATORS_H_

#include <cstdint>
#include <string_view>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace gb {

//==============================================================================
// Common validators
//
// Each of these is a CliValidator for a flag's or argument's `validators`:
//
//   const gb::CliFlag<int64_t> kThreads = {
//       .name = "threads",
//       .validators = {gb::CliInRange<1, 64>},
//   };
//
// The number validators are function templates whose bounds are template
// arguments, so the bounds are part of the constant. Their value type is
// deduced from the flag or argument they are listed in, and the bounds may be
// integers or floating point for either (`gb::CliAtLeast<0.5>`).
//==============================================================================

namespace internal {

// Whether `T` is a number type a flag or argument can have, which the number
// validators take.
template <typename T>
inline constexpr bool kIsCliNumber =
    std::is_same_v<T, int64_t> || std::is_same_v<T, double>;

}  // namespace internal

// Validates that a number is from `kMin` to `kMax`, inclusive.
template <auto kMin, auto kMax, typename T>
absl::Status CliInRange(const T& value) {
  static_assert(internal::kIsCliNumber<T>, "CliInRange is for numbers");
  static_assert(kMin <= kMax, "CliInRange's minimum is more than its maximum");
  if (value < kMin || value > kMax) {
    return absl::InvalidArgumentError(
        absl::StrCat(value, " is not from ", kMin, " to ", kMax));
  }
  return absl::OkStatus();
}

// Validates that a number is at least `kMin`.
template <auto kMin, typename T>
absl::Status CliAtLeast(const T& value) {
  static_assert(internal::kIsCliNumber<T>, "CliAtLeast is for numbers");
  if (value < kMin) {
    return absl::InvalidArgumentError(
        absl::StrCat(value, " is less than ", kMin));
  }
  return absl::OkStatus();
}

// Validates that a number is at most `kMax`.
template <auto kMax, typename T>
absl::Status CliAtMost(const T& value) {
  static_assert(internal::kIsCliNumber<T>, "CliAtMost is for numbers");
  if (value > kMax) {
    return absl::InvalidArgumentError(
        absl::StrCat(value, " is more than ", kMax));
  }
  return absl::OkStatus();
}

// Validates that a string is not empty.
absl::Status CliNotEmpty(const std::string_view& value);

}  // namespace gb

#endif  // GB_CLI_CLI_VALIDATORS_H_
