// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONTAINER_ARRAY_H_
#define GB_CONTAINER_ARRAY_H_

//==============================================================================
// These classes encapsulate a fixed size potentially multi-dimensional array.
//
// These classes satisfy standard C++17 container requirements Container and
// ReversibleContainer (except that it is default-constructed to be full, and
// the complexity of swap is linear), and also satisfy the requirements of
// ContiguousContainer. Unlike std::array, these do not satisfy any additional
// requirements of SequenceContainer.
//
// The Array family of classes are implemented in terms of std::array. However,
// unlike std::array, it is never "uninitialized" even for POD types: elements
// are value-initialized, so any type without a user-provided default
// constructor is zero initialized first.
//
// Beyond standard container requirements, these containers do not provide
// additional accessors except for the following:
//  - fill(value_type): This fills the entire array with the specified value
//    (same as std::array).
//  - dim(): This returns the dimensions of the array (separate from size()
//    which gives the total number of items in the array).
//  - data(): This returns a pointer to the raw underlying array (same as
//    std::array).
//  - int_index(index_type): This returns the linear integer index given a value
//    of the specified index type. This can be used to index relative to the
//    pointer returned by data().
//  - operator[]: Provides direct access to a value in the array via the
//    designated index type of the array (int for Array, glm::ivec2 for Array2d,
//    etc.)
//  - operator(): Like operator[], provides direct access to a value in the
//    array. However this supports both the index type or individual integer
//    indexes.
//
// Multidimensional arrays also support multiple memory ordering options. For
// instance a 2D array can be laid out where adjacent X values are adjacent in
// memory (Y major memory order), or where adjacent X values are separated by
// YSize values in memory (X major memory order).
//==============================================================================

#include <array>

#include "glm/glm.hpp"

namespace gb {

namespace internal {

// Derived is the array class, so that swap and comparison only accept another
// array with the same layout.
template <typename Derived, typename Type, int Size>
class BaseArray : private std::array<Type, Size> {
 public:
  using Base = typename std::array<Type, Size>;

  // Value-initializes every element.
  constexpr BaseArray() : Base() {}

  // Container types
  using Base::const_iterator;
  using Base::const_pointer;
  using Base::const_reference;
  using Base::difference_type;
  using Base::iterator;
  using Base::pointer;
  using Base::reference;
  using Base::reverse_iterator;
  using Base::size_type;
  using Base::value_type;

  // Container iteration
  using Base::begin;
  using Base::end;

  using Base::cbegin;
  using Base::cend;

  using Base::rbegin;
  using Base::rend;

  using Base::crbegin;
  using Base::crend;

  // Container capacity
  using Base::empty;
  using Base::max_size;
  using Base::size;

  // container operations
  using Base::fill;
  constexpr void swap(Derived& other) { Base::swap(other); }
  friend constexpr void swap(Derived& a, Derived& b) { a.swap(b); }
  friend constexpr bool operator==(const Derived& a, const Derived& b) {
    return static_cast<const Base&>(a) == static_cast<const Base&>(b);
  }

 protected:
  // Make available to derived classes
  using Base::data;
};

}  // namespace internal

// Single dimensional array.
//
// Note: There is no advantage of this over std::array as it is strictly more
// limited, it is only provided here for parity / consistency.
template <typename Type, int Size>
class Array : public internal::BaseArray<Array<Type, Size>, Type, Size> {
 public:
  using Base = typename internal::BaseArray<Array, Type, Size>;
  using index_type = int;

  constexpr index_type dim() const { return Size; }

  using Base::data;
  constexpr int int_index(index_type i) const { return i; }

  constexpr const Type& operator[](index_type i) const { return data()[i]; }
  constexpr Type& operator[](index_type i) { return data()[i]; }

  constexpr const Type& operator()(index_type i) const { return data()[i]; }
  constexpr Type& operator()(index_type i) { return data()[i]; }
};

// Two dimensional arrays.
//
// ArrayXY stores data in X major memory order. This is equivalent to:
//    Type[XSize][YSize]
// ArrayYX stores data in Y major memory order. This is equivalent to:
//    Type[YSize][XSize]
//
// Array2d is an alias for ArrayYX, as this is generally what is useful in
// games.
//
// X major order is what may be expected in comparison to the equivalent raw
// array declaration, and Y major is what is generally expected in many 2D
// representations (for instance a 2D image).
template <typename Type, int XSize, int YSize = XSize>
class ArrayXY : public internal::BaseArray<ArrayXY<Type, XSize, YSize>, Type,
                                           XSize * YSize> {
 public:
  using Base = typename internal::BaseArray<ArrayXY, Type, XSize * YSize>;
  using index_type = glm::ivec2;

  constexpr index_type dim() const { return {XSize, YSize}; }

  using Base::data;
  constexpr int int_index(int x, int y) const { return x * YSize + y; }
  constexpr int int_index(const index_type& i) const {
    return int_index(i.x, i.y);
  }

  constexpr const Type& operator[](const index_type& i) const {
    return data()[int_index(i)];
  }
  constexpr Type& operator[](const index_type& i) {
    return data()[int_index(i)];
  }

  constexpr const Type& operator()(const index_type& i) const {
    return data()[int_index(i)];
  }
  constexpr Type& operator()(const index_type& i) {
    return data()[int_index(i)];
  }
  constexpr const Type& operator()(int x, int y) const {
    return data()[int_index(x, y)];
  }
  constexpr Type& operator()(int x, int y) { return data()[int_index(x, y)]; }
};

template <typename Type, int XSize, int YSize = XSize>
class ArrayYX : public internal::BaseArray<ArrayYX<Type, XSize, YSize>, Type,
                                           XSize * YSize> {
 public:
  using Base = typename internal::BaseArray<ArrayYX, Type, XSize * YSize>;
  using index_type = glm::ivec2;

  constexpr index_type dim() const { return {XSize, YSize}; }

  using Base::data;
  constexpr int int_index(int x, int y) const { return y * XSize + x; }
  constexpr int int_index(const index_type& i) const {
    return int_index(i.x, i.y);
  }

  constexpr const Type& operator[](const index_type& i) const {
    return data()[int_index(i)];
  }
  constexpr Type& operator[](const index_type& i) {
    return data()[int_index(i)];
  }

  constexpr const Type& operator()(const index_type& i) const {
    return data()[int_index(i)];
  }
  constexpr Type& operator()(const index_type& i) {
    return data()[int_index(i)];
  }
  constexpr const Type& operator()(int x, int y) const {
    return data()[int_index(x, y)];
  }
  constexpr Type& operator()(int x, int y) { return data()[int_index(x, y)]; }
};

template <typename Type, int XSize, int YSize = XSize>
using Array2d = ArrayYX<Type, XSize, YSize>;

// Three dimensional arrays.
//
// ArrayXYZ stores data in X major memory order. This is equivalent to:
//    Type[XSize][YSize][ZSize]
// ArrayZYX stores data in Z major memory order. This is equivalent to:
//    Type[ZSize][YSize][XSize]
//
// Array3d is an alias for ArrayZYX, as this is generally what is useful in
// games.
//
// X major order is what may be expected in comparison to the equivalent raw
// array declaration, and Z major is what is generally expected in many 3D
// representations (for instance, an array of 2D images).
template <typename Type, int XSize, int YSize = XSize, int ZSize = XSize>
class ArrayXYZ : public internal::BaseArray<ArrayXYZ<Type, XSize, YSize, ZSize>,
                                            Type, XSize * YSize * ZSize> {
 public:
  using Base =
      typename internal::BaseArray<ArrayXYZ, Type, XSize * YSize * ZSize>;
  using index_type = glm::ivec3;

  constexpr index_type dim() const { return {XSize, YSize, ZSize}; }

  using Base::data;
  constexpr int int_index(int x, int y, int z) const {
    return (x * YSize + y) * ZSize + z;
  }
  constexpr int int_index(const index_type& i) const {
    return int_index(i.x, i.y, i.z);
  }

  constexpr const Type& operator[](const index_type& i) const {
    return data()[int_index(i)];
  }
  constexpr Type& operator[](const index_type& i) {
    return data()[int_index(i)];
  }

  constexpr const Type& operator()(const index_type& i) const {
    return data()[int_index(i)];
  }
  constexpr Type& operator()(const index_type& i) {
    return data()[int_index(i)];
  }
  constexpr const Type& operator()(int x, int y, int z) const {
    return data()[int_index(x, y, z)];
  }
  constexpr Type& operator()(int x, int y, int z) {
    return data()[int_index(x, y, z)];
  }
};

template <typename Type, int XSize, int YSize = XSize, int ZSize = XSize>
class ArrayZYX : public internal::BaseArray<ArrayZYX<Type, XSize, YSize, ZSize>,
                                            Type, XSize * YSize * ZSize> {
 public:
  using Base =
      typename internal::BaseArray<ArrayZYX, Type, XSize * YSize * ZSize>;
  using index_type = glm::ivec3;

  constexpr index_type dim() const { return {XSize, YSize, ZSize}; }

  using Base::data;
  constexpr int int_index(int x, int y, int z) const {
    return (z * YSize + y) * XSize + x;
  }
  constexpr int int_index(const index_type& i) const {
    return int_index(i.x, i.y, i.z);
  }

  constexpr const Type& operator[](const index_type& i) const {
    return data()[int_index(i)];
  }
  constexpr Type& operator[](const index_type& i) {
    return data()[int_index(i)];
  }

  constexpr const Type& operator()(const index_type& i) const {
    return data()[int_index(i)];
  }
  constexpr Type& operator()(const index_type& i) {
    return data()[int_index(i)];
  }
  constexpr const Type& operator()(int x, int y, int z) const {
    return data()[int_index(x, y, z)];
  }
  constexpr Type& operator()(int x, int y, int z) {
    return data()[int_index(x, y, z)];
  }
};

template <typename Type, int XSize, int YSize = XSize, int ZSize = XSize>
using Array3d = ArrayZYX<Type, XSize, YSize, ZSize>;

}  // namespace gb

#endif  // GB_CONTAINER_ARRAY_H_
