// Copyright (c) 2022 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_COLLIDE_SHAPES_H
#define GB_COLLIDE_SHAPES_H

#include <algorithm>

#include "glm/glm.hpp"

namespace gb {

//==============================================================================
// Convenience methods to calculate properties of a shape
//==============================================================================

// Assumes counterclockwise order of vertices.
inline glm::vec3 GetTriangleNormal(const glm::vec3& v0, const glm::vec3& v1,
                                   const glm::vec3& v2) {
  return glm::normalize(glm::cross(v2 - v0, v0 - v1));
}

// Returns the point which is the component-wise minimum for of set of points.
inline glm::vec3 GetMinBounds(const glm::vec3& a, const glm::vec3& b) {
  return glm::min(a, b);
}
inline glm::vec3 GetMinBounds(const glm::vec3& a, const glm::vec3& b,
                              const glm::vec3& c) {
  return glm::min(glm::min(a, b), c);
}

// Returns the point which is the component-wise maximum for of set of points.
inline glm::vec3 GetMaxBounds(const glm::vec3& a, const glm::vec3& b) {
  return glm::max(a, b);
}
inline glm::vec3 GetMaxBounds(const glm::vec3& a, const glm::vec3& b,
                              const glm::vec3& c) {
  return glm::max(glm::max(a, b), c);
}

//==============================================================================
// All collision shapes are prefixed with "C" to denote their association as a
// collision shape. However they can also be used more generally, as they
// are inherently minimal representations.
//
// These shapes are generally optional for collision routines, but are
// conveniences when a shape needs to be cached or reused.
//==============================================================================

// A ray is defined by an origin point and direction vector.
//
// For most collision functions, the direction vector must be normalized. If it
// does *not* need to be normalized, that will be noted in the function docs.
struct CRay {
  CRay() = default;
  CRay(const glm::vec3& in_origin, const glm::vec3& in_dir)
      : origin(in_origin), dir(in_dir) {}

  glm::vec3 origin;
  glm::vec3 dir;
};

// A line is defined by two points (a start and end).
struct CLine {
  CLine() = default;
  CLine(const glm::vec3& v0, const glm::vec3& v1) : vertex{v0, v1} {}
  CLine(const glm::vec3 v[2]) : vertex{v[0], v[1]} {}

  glm::vec3 vertex[2];
};

// A triangle is defined by three vertices.
//
// For basic collision the winding order does not matter. However, if a normal
// is required (aka there is a front and back), then the vertices are expected
// to be in counterclockwise order.
struct CTriangle {
  CTriangle() = default;
  CTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
      : vertex{v0, v1, v2} {}
  CTriangle(const glm::vec3 v[3]) : vertex{v[0], v[1], v[2]} {}

  glm::vec3 vertex[3];
};

// A triangle defined by three vertices and a normal.
//
// This is the same as CTriangle but also stores the normal for the triangle.
// For use with collision routines, the normal must be normalizes and correct
// for the stored vertices.
struct CNormalTriangle : CTriangle {
  CNormalTriangle() = default;
  CNormalTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                  const glm::vec3& in_normal)
      : CTriangle(v0, v1, v2), normal(in_normal) {}
  CNormalTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
      : CTriangle(v0, v1, v2), normal(GetTriangleNormal(v0, v1, v2)) {}
  explicit CNormalTriangle(const CTriangle& triangle)
      : CTriangle(triangle),
        normal(GetTriangleNormal(triangle.vertex[0], triangle.vertex[1],
                                 triangle.vertex[2])) {}

  glm::vec3 normal;
};

// A sphere is defined by center and radius.
struct CSphere {
  CSphere() = default;
  CSphere(const glm::vec3& in_center, float in_radius)
      : center(in_center), radius(in_radius) {}

  glm::vec3 center;
  float radius;
};

// An axis-aligned bounding box is defined by the volume [pos, pos + size].
struct CAabb {
  CAabb() = default;
  CAabb(const glm::vec3& in_pos, const glm::vec3& in_size)
      : pos(in_pos), size(in_size) {}
  explicit CAabb(const CLine& line);
  explicit CAabb(const CTriangle& triangle);
  explicit CAabb(const CSphere& sphere);

  glm::vec3 pos;
  glm::vec3 size;
};

}  // namespace gb

#endif  // GB_COLLIDE_SHAPES_H
