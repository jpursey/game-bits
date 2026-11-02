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
#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"

namespace gb {

namespace internal {

template <typename T, auto kMin, auto kMax>
absl::Status ValidateInRange(const ValidatedContext&, const T& value) {
  static_assert(std::is_same_v<T, int64_t> || std::is_same_v<T, double>,
                "kCliInRange is for numbers");
  static_assert(kMin <= kMax, "kCliInRange's minimum is more than its maximum");
  if (value < kMin || value > kMax) {
    return absl::InvalidArgumentError(
        absl::StrCat(value, " is not from ", kMin, " to ", kMax));
  }
  return absl::OkStatus();
}

template <typename T, auto kMin>
absl::Status ValidateAtLeast(const ValidatedContext&, const T& value) {
  static_assert(std::is_same_v<T, int64_t> || std::is_same_v<T, double>,
                "kCliAtLeast is for numbers");
  if (value < kMin) {
    return absl::InvalidArgumentError(
        absl::StrCat(value, " is less than ", kMin));
  }
  return absl::OkStatus();
}

template <typename T, auto kMax>
absl::Status ValidateAtMost(const ValidatedContext&, const T& value) {
  static_assert(std::is_same_v<T, int64_t> || std::is_same_v<T, double>,
                "kCliAtMost is for numbers");
  if (value > kMax) {
    return absl::InvalidArgumentError(
        absl::StrCat(value, " is more than ", kMax));
  }
  return absl::OkStatus();
}

}  // namespace internal

//==============================================================================
// Common validators
//
// Each of these is a CliValidator, which a flag or argument lists by address:
//
//   const gb::CliFlag<int64_t> kThreads = {
//       .name = "threads",
//       .validators = {&gb::kCliInRange<int64_t, 1, 64>},
//   };
//
// The number validators are variable templates of the value type, which must
// be the flag's or argument's, and their bounds, so the bounds are part of the
// constant. The bounds may be integers or floating point for either type
// (`gb::kCliAtLeast<double, 1>`).
//==============================================================================

// Validates that a number is from `kMin` to `kMax`, inclusive.
template <typename T, auto kMin, auto kMax>
inline const CliValidator<T> kCliInRange = {
    .validate = internal::ValidateInRange<T, kMin, kMax>,
};

// Validates that a number is at least `kMin`.
template <typename T, auto kMin>
inline const CliValidator<T> kCliAtLeast = {
    .validate = internal::ValidateAtLeast<T, kMin>,
};

// Validates that a number is at most `kMax`.
template <typename T, auto kMax>
inline const CliValidator<T> kCliAtMost = {
    .validate = internal::ValidateAtMost<T, kMax>,
};

// Validates that a string is not empty.
extern const CliValidator<std::string_view> kCliNotEmpty;

}  // namespace gb

#endif  // GB_CLI_CLI_VALIDATORS_H_
