// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_CALLBACK_H_
#define GB_BASE_CALLBACK_H_

#include "absl/functional/any_invocable.h"

namespace gb {

// Deprecated: use absl::AnyInvocable directly. This alias remains only until
// projects built on Game Bits have moved off it.
//
// Defines a callback to any callable type with the signature Sig, such as
// `Callback<int(float)>`. It accepts callables that are move-only (such as a
// lambda that captures a std::unique_ptr), and is itself move-only.
//
// To use it correctly:
// - A callback is only called through a non-const reference, unless Sig is
//   const-qualified (such as `Callback<void() const>`), in which case it only
//   accepts callables that can be called as const. A lambda that owns a
//   callback and calls it must therefore be `mutable`.
// - A callback owns a copy of what it is given. To refer to a callable owned
//   elsewhere, pass `std::ref(callable)`, which must outlive the callback.
// - A callable that cannot be moved can be constructed in place:
//   `Callback<void()> callback(absl::in_place_type<Type>, args...)`.
// - A default constructed callback is empty, and compares equal to nullptr.
//   Calling an empty callback is undefined behavior.
// - A moved-from callback is in a valid but unspecified state. Assign to it
//   (such as `= nullptr`) before relying on whether it is empty.
template <typename Sig>
using Callback = absl::AnyInvocable<Sig>;

}  // namespace gb

#endif  // GB_BASE_CALLBACK_H_
