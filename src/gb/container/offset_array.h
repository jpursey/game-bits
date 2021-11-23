// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONTAINER_OFFSET_ARRAY_H_
#define GB_CONTAINER_OFFSET_ARRAY_H_

#include "glm/glm.hpp"
#include "glog/logging.h"

namespace gb {

// This class provides access to an array with an implied offset to every
// indexing operation.
//
// The underlying type must support default construction.
template <typename Type, int Offset, int Size>
class OffsetArray {
 public:
  OffsetArray() = default;

  const Type& operator()(int i) const { return value_[i + Offset]; }
  Type& operator()(int i) { return value_[i + Offset]; }

 private:
  Type value_[Size] = {};
};

// This class provides access to a 2D array of equal size dimentions with an
// implied offset to every indexing operation in all dimensions.
//
// The underlying type must support default construction.
template <typename Type, int Offset, int Size>
class OffsetArray2d {
 public:
  OffsetArray2d() = default;

  const Type& operator()(int x, int y) const {
    return value_[x + Offset][y + Offset];
  }
  Type& operator()(int x, int y) { return value_[x + Offset][y + Offset]; }

  const Type& operator()(const glm::ivec2& pos) const {
    return value_[pos.x + Offset][pos.y + Offset];
  }
  Type& operator()(const glm::ivec2& pos) {
    return value_[pos.x + Offset][pos.y + Offset];
  }

 private:
  Type value_[Size][Size] = {};
};

// This class provides access to a 3D array of equal size dimentions with an
// implied offset to every indexing operation in all dimensions.
//
// The underlying type must support default construction.
template <typename Type, int Offset, int Size>
class OffsetArray3d {
 public:
  OffsetArray3d() = default;

  const Type& operator()(int x, int y, int z) const {
    return value_[x + Offset][y + Offset][z + Offset];
  }
  Type& operator()(int x, int y, int z) {
    return value_[x + Offset][y + Offset][z + Offset];
  }

  const Type& operator()(const glm::ivec3& pos) const {
    return value_[pos.x + Offset][pos.y + Offset][pos.z + Offset];
  }
  Type& operator()(const glm::ivec3& pos) {
    return value_[pos.x + Offset][pos.y + Offset][pos.z + Offset];
  }

 private:
  Type value_[Size][Size][Size] = {};
};

}  // namespace gb

#endif  // GB_CONTAINER_OFFSET_ARRAY_H_
