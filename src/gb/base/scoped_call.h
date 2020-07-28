// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_SCOPED_CALL_H_
#define GB_BASE_SCOPED_CALL_H_

namespace gb {

// This is a simple RAII class that wraps a callable function (usually a local
// lambda) to ensure it gets called when the ScopedCall is destructed.
template <typename Callable>
class ScopedCall final {
 public:
  ScopedCall(const Callable& callback) : callback_(callback) {}
  ScopedCall(Callable&& callback) : callback_(std::move(callback)) {}
  ~ScopedCall() { callback_(); }

 private:
  Callable callback_;
};

}  // namespace gb

#endif  // GB_BASE_SCOPED_CALL_H_
