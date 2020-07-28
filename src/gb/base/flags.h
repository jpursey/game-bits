// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_FLAGS_H_
#define GB_BASE_FLAGS_H_

#include <stdint.h>

#include <initializer_list>
#include <type_traits>

namespace gb {

// This class allows a C++ enumeration of up to 64 values to be used as a
// type-safe flag set.
//
// Individual flags can be set or cleared individually or in groups. Union and
// intersection operations are also supported. All construction and
// non-modification functions are constexpr and can be used at compiple time.
// The class is as lightweight as an integer, and is intended to be passed by
// value.
//
// This class is thread-compatible.
template <typename FlagType>
class Flags {
 public:
  // Constructs a set with no flags.
  constexpr Flags() : value_(0) {}

  // Explicitly constructs flags from a mask value.
  //
  // This should only be used for low level operations, when the raw underlying
  // value is required.
  constexpr explicit Flags(uint64_t value) : value_(value) {}

  // Initialize from a single flag.
  constexpr Flags(FlagType flag) : value_(ToValue(flag)) {}

  // Initialize from a set of flags in initializer list form.
  constexpr Flags(std::initializer_list<Flags<FlagType>> flags) : value_(0) {
    for (auto it = flags.begin(); it != flags.end(); ++it) {
      value_ |= it->value_;
    }
  }

  // Initialize from a set of flags as arguments.
  template <typename... RemainingFlags>
  constexpr Flags(Flags<FlagType> a, Flags<FlagType> b,
                  RemainingFlags... remaining)
      : value_(a.value_ | b.value_ | Flags<FlagType>({remaining...}).value_) {}

  // Returns the underlying bit mask for this flag set.
  //
  // This should only be used for low level operations, when the raw underlying
  // value is required.
  constexpr uint64_t GetMask() const { return value_; }

  // Returns true if no flags are in the set.
  constexpr bool IsEmpty() const { return value_ == 0; }

  // Returns true if *all* the specified flags are in the set.
  //
  // This will always return true if an empty flag set is passed in.
  constexpr bool IsSet(Flags<FlagType> flags) const {
    return (flags.value_ & value_) == flags.value_;
  }

  // Returns true if *any* of the specified flags are in the set.
  //
  // This will always return false if an empty flag set is passed in.
  constexpr bool Intersects(Flags<FlagType> flags) const {
    return (flags.value_ & value_) != 0;
  }

  // Adds the specified flags to the set, if they are not already.
  void Set(Flags<FlagType> flags) { value_ |= flags.value_; }

  // Clears all flags in the set, leaving it empty.
  void Clear() { value_ = 0; }

  // Clears the specified flags from the set, if they exist.
  void Clear(Flags<FlagType> flags) { value_ &= ~flags.value_; }

  // Adds the specified flags to the set, if they are not already.
  Flags<FlagType>& operator+=(Flags<FlagType> other) {
    Set(other);
    return *this;
  }

  // Clears the specified flags from the set, if they exist.
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

// Returns the union of the two flag sets.
template <typename FlagType>
constexpr Flags<FlagType> Union(Flags<FlagType> a, Flags<FlagType> b) {
  return Flags<FlagType>(a.GetMask() | b.GetMask());
}
template <typename FlagType>
constexpr Flags<FlagType> Union(FlagType a, Flags<FlagType> b) {
  return Flags<FlagType>(Flags<FlagType>(a).GetMask() | b.GetMask());
}
template <typename FlagType>
constexpr Flags<FlagType> Union(Flags<FlagType> a, FlagType b) {
  return Flags<FlagType>(a.GetMask() | Flags<FlagType>(b).GetMask());
}

// Returns the intersection of the two flag sets.
template <typename FlagType>
constexpr Flags<FlagType> Intersect(Flags<FlagType> a, Flags<FlagType> b) {
  return Flags<FlagType>(a.GetMask() & b.GetMask());
}
template <typename FlagType>
constexpr Flags<FlagType> Intersect(FlagType a, Flags<FlagType> b) {
  return Flags<FlagType>(Flags<FlagType>(a).GetMask() & b.GetMask());
}
template <typename FlagType>
constexpr Flags<FlagType> Intersect(Flags<FlagType> a, FlagType b) {
  return Flags<FlagType>(a.GetMask() & Flags<FlagType>(b).GetMask());
}

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
  return Union(a, b);
}
template <typename FlagType>
constexpr Flags<FlagType> operator+(Flags<FlagType> a, FlagType b) {
  return Union(a, b);
}
template <typename FlagType>
constexpr Flags<FlagType> operator+(FlagType a, Flags<FlagType> b) {
  return Union(a, b);
}
template <typename FlagType>
constexpr Flags<FlagType> operator-(Flags<FlagType> a, Flags<FlagType> b) {
  return Flags<FlagType>(a.GetMask() & ~b.GetMask());
}
template <typename FlagType>
constexpr Flags<FlagType> operator-(Flags<FlagType> a, FlagType b) {
  return Flags<FlagType>(a.GetMask() & ~Flags<FlagType>(b).GetMask());
}

}  // namespace gb

#endif  // GB_BASE_FLAGS_H_
