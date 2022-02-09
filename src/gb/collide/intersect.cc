// Copyright (c) 2022 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/collide/intersect.h"

#include "glog/logging.h"

namespace gb {

namespace {

inline bool IsNormalized(const glm::vec3& v) {
  const float dist_sq = glm::dot(v, v);
  return dist_sq > 1.0f - kCEpsilon && dist_sq < 1.0f + kCEpsilon;
}

inline float LengthSquared(const glm::vec3& v) { return glm::dot(v, v); }

// Returns true if the point is in the sphere and calculates the squared
// distance of that point to the center of the sphere.
bool IsPointInSphereImpl(const glm::vec3& p, const glm::vec3& center,
                         float radius_sq) {
  return LengthSquared(p - center) < radius_sq;
}

bool IsRayInSphere(const glm::vec3& origin, const glm::vec3& dir,
                   const glm::vec3& center, float radius_sq, float& t) {
  t = glm::dot(center - origin, dir);
  if (t < 0.0f) {
    // Closest point is before the ray origin, so check if the ray origin is in
    // the sphere.
    return IsPointInSphereImpl(origin, center, radius_sq);
  }
  // The closest point on the ray to the sphere center is beyond the start of
  // the ray. If the closest point is is within the radius of the sphere, we
  // have a hit.
  return IsPointInSphereImpl(origin + dir * t, center, radius_sq);
}

bool IsLineInSphere(const glm::vec3& l0, const glm::vec3& l1,
                    const glm::vec3& center, float radius_sq) {
  float t;
  const glm::vec3 dir = l1 - l0;
  if (!IsRayInSphere(l0, dir, center, radius_sq, t)) {
    return false;
  }
  return t < 1.0f || IsPointInSphereImpl(l1, center, radius_sq);
}

}  // namespace

bool IsPointInTriangle(const glm::vec3& p, const glm::vec3& v0,
                       const glm::vec3& v1, const glm::vec3& v2) {
  const glm::vec3 ov0 = v0 - p;
  const glm::vec3 ov1 = v1 - p;
  const glm::vec3 ov2 = v2 - p;
  const glm::vec3 n0 = glm::cross(ov1, ov2);
  const glm::vec3 n1 = glm::cross(ov2, ov0);
  const glm::vec3 n2 = glm::cross(ov0, ov1);
  return glm::dot(n0, n1) >= 0.0f && glm::dot(n0, n2) >= 0.0f;
}

bool IsPointInSphere(const glm::vec3& p, const glm::vec3& center,
                     float radius) {
  return IsPointInSphereImpl(p, center, radius * radius);
}

// Moller-Trumbore intersection algorithm from Wikipedia:
// https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
bool RayHitsTriangleAt(const glm::vec3& origin, const glm::vec3& dir,
                       const glm::vec3& v0, const glm::vec3& v1,
                       const glm::vec3& v2, float& out_t) {
  const glm::vec3 edge1 = v1 - v0;
  const glm::vec3 edge2 = v2 - v0;
  const glm::vec3 h = glm::cross(dir, edge2);
  const float a = glm::dot(edge1, h);
  if (a > -kCEpsilon && a < kCEpsilon) {
    return false;
  }
  const float f = 1.0f / a;
  const glm::vec3 s = origin - v0;
  const float u = f * glm::dot(s, h);
  if (u < 0.0f || u > 1.0f) {
    return false;
  }
  const glm::vec3 q = glm::cross(s, edge1);
  const float v = f * glm::dot(dir, q);
  if (v < 0.0f || u + v > 1.0f) {
    return false;
  }
  float t = f * glm::dot(edge2, q);
  if (t > kCEpsilon) {
    out_t = t;
    return true;
  }
  return false;
}

bool RayHitsSphere(const glm::vec3& origin, const glm::vec3& dir,
                   const glm::vec3& center, float radius) {
  float t;
  return IsRayInSphere(origin, dir, center, radius * radius, t);
}

bool LineHitsSphere(const glm::vec3& l0, const glm::vec3& l1,
                    const glm::vec3& center, float radius) {
  return IsLineInSphere(l0, l1, center, radius * radius);
}

bool SphereHitsTriangle(const glm::vec3& center, float radius,
                        const glm::vec3& v0, const glm::vec3& v1,
                        const glm::vec3& v2, const glm::vec3& normal) {
  DCHECK(IsNormalized(normal));

  // Quick reject. If we don't intersect the plane, then we don't intersect the
  // triangle.
  const float plane_distance = glm::dot(normal, center - v0);
  if (plane_distance > radius - kCEpsilon) {
    return false;
  }

  const float radius_sq = radius * radius - kCEpsilon;
  return
      // If any triangle corner is in the sphere, then we intersect.
      IsPointInSphereImpl(v0, center, radius_sq) ||
      IsPointInSphereImpl(v1, center, radius_sq) ||
      IsPointInSphereImpl(v2, center, radius_sq) ||
      // If the sphere's intersection with the plane is in the triangle, then we
      // intersect.
      IsPointInTriangle(center - normal * plane_distance, v0, v1, v2) ||
      // Now we need to check each triangle edge. If any edge intersects the
      // sphere, then we intersect, otherwise we do not.
      IsLineInSphere(v0, v1, center, radius_sq) ||
      IsLineInSphere(v0, v2, center, radius_sq) ||
      IsLineInSphere(v1, v2, center, radius_sq);
}

}  // namespace gb
