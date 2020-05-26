#ifndef GBITS_BASE_SCOPED_CALL_H_
#define GBITS_BASE_SCOPED_CALL_H_

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

#endif  // GBITS_BASE_SCOPED_CALL_H_
