// Copyright (c) 2022 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/collide/shapes.h"

#include <algorithm>

namespace gb {

CAabb::CAabb(const CLine& line)
    : pos(GetMinBounds(line.vertex[0], line.vertex[1])) {
  size = GetMaxBounds(line.vertex[0], line.vertex[1]) - pos;
}

CAabb::CAabb(const CTriangle& triangle)
    : pos(GetMinBounds(triangle.vertex[0], triangle.vertex[1],
                       triangle.vertex[2])) {
  size =
      GetMaxBounds(triangle.vertex[0], triangle.vertex[1], triangle.vertex[2]) -
      pos;
}

CAabb::CAabb(const CSphere& sphere) {
  const glm::vec3 radius = {sphere.radius, sphere.radius, sphere.radius};
  pos = sphere.center - radius;
  size = radius * 2.0f;
}

}  // namespace gb
