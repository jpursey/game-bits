#ifndef GBITS_BASE_FLAGS_H_
#define GBITS_BASE_FLAGS_H_

#include <stdint.h>

#include <initializer_list>
#include <type_traits>

namespace gb {

template <typename FlagType>
class Flags {
 public:
  constexpr Flags() : value_(0) {}
  constexpr explicit Flags(uint64_t value) : value_(value) {}
  constexpr Flags(FlagType flag) : value_(ToValue(flag)) {}
  constexpr Flags(std::initializer_list<Flags<FlagType>> flags) : value_(0) {
    for (auto it = flags.begin(); it != flags.end(); ++it) {
      value_ |= it->value_;
    }
  }
  template <typename... RemainingFlags>
  constexpr Flags(Flags<FlagType> a, Flags<FlagType> b,
                  RemainingFlags... remaining)
      : value_(a.value_ | b.value_ | Flags<FlagType>({remaining...}).value_) {}

  constexpr uint64_t GetMask() const { return value_; }
  constexpr bool IsEmpty() const { return value_ == 0; }
  constexpr bool IsSet(Flags<FlagType> flags) const {
    return (flags.value_ & value_) == flags.value_;
  }
  constexpr bool Intersects(Flags<FlagType> flags) const {
    return (flags.value_ & value_) != 0;
  }

  void Set(Flags<FlagType> flags) { value_ |= flags.value_; }
  void Clear() { value_ = 0; }
  void Clear(Flags<FlagType> flags) { value_ &= ~flags.value_; }

  constexpr Flags<FlagType> Union(Flags<FlagType> other) {
    return Flags<FlagType>(value_ | other.value_);
  }
  constexpr Flags<FlagType> Intersect(Flags<FlagType> other) {
    return Flags<FlagType>(value_ & other.value_);
  }
  constexpr Flags<FlagType> Remove(Flags<FlagType> other) {
    return Flags<FlagType>(value_ & ~other.value_);
  }

  Flags<FlagType>& operator+=(Flags<FlagType> other) {
    Set(other);
    return *this;
  }
  Flags<FlagType>& operator-=(Flags<FlagType> other) {
    Clear(other);
    return *this;
  }

 private:
  static constexpr uint64_t ToValue(FlagType flag) {
    return 1ULL << static_cast<uint64_t>(
               static_cast<std::underlying_type_t<FlagType>>(flag));
  }

  uint64_t value_;
};

template <typename FlagType>
constexpr bool operator==(Flags<FlagType> a, Flags<FlagType> b) {
  return a.GetMask() == b.GetMask();
}
template <typename FlagType>
constexpr bool operator==(Flags<FlagType> a, FlagType b) {
  return a.GetMask() == Flags<FlagType>(b).GetMask();
}
template <typename FlagType>
constexpr bool operator==(FlagType a, Flags<FlagType> b) {
  return Flags<FlagType>(a).GetMask() == b.GetMask();
}
template <typename FlagType>
constexpr bool operator!=(Flags<FlagType> a, Flags<FlagType> b) {
  return a.GetMask() != b.GetMask();
}
template <typename FlagType>
constexpr bool operator!=(Flags<FlagType> a, FlagType b) {
  return a.GetMask() != Flags<FlagType>(b).GetMask();
}
template <typename FlagType>
constexpr bool operator!=(FlagType a, Flags<FlagType> b) {
  return Flags<FlagType>(a).GetMask() != b.GetMask();
}
template <typename FlagType>
constexpr bool operator<(Flags<FlagType> a, Flags<FlagType> b) {
  return a.GetMask() < b.GetMask();
}
template <typename FlagType>
constexpr bool operator>(Flags<FlagType> a, Flags<FlagType> b) {
  return a.GetMask() > b.GetMask();
}
template <typename FlagType>
constexpr bool operator<=(Flags<FlagType> a, Flags<FlagType> b) {
  return a.GetMask() <= b.GetMask();
}
template <typename FlagType>
constexpr bool operator>=(Flags<FlagType> a, Flags<FlagType> b) {
  return a.GetMask() >= b.GetMask();
}
template <typename FlagType>
constexpr Flags<FlagType> operator+(Flags<FlagType> a, Flags<FlagType> b) {
  return a.Union(b);
}
template <typename FlagType>
constexpr Flags<FlagType> operator+(Flags<FlagType> a, FlagType b) {
  return a.Union(b);
}
template <typename FlagType>
constexpr Flags<FlagType> operator+(FlagType a, Flags<FlagType> b) {
  return b.Union(a);
}
template <typename FlagType>
constexpr Flags<FlagType> operator-(Flags<FlagType> a, Flags<FlagType> b) {
  return a.Remove(b);
}
template <typename FlagType>
constexpr Flags<FlagType> operator-(Flags<FlagType> a, FlagType b) {
  return a.Remove(b);
}

}  // namespace gb

#endif  // GBITS_BASE_FLAGS_H_
