// Copyright (c) 2022 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/collide/intersect.h"

#include <cmath>
#include <limits>

#undef _CRT_SECURE_NO_WARNINGS
#include "absl/log/check.h"
#include "ccd/ccd.h"

namespace gb {

namespace {

// Allows for the rounding error in a normalized float vector, which is several
// times the float epsilon.
constexpr float kNormalizedEpsilon = 0.00001f;

inline float LengthSquared(const glm::vec3& v) { return glm::dot(v, v); }

inline bool IsNormalized(const glm::vec3& v) {
  return std::abs(LengthSquared(v) - 1.0f) < kNormalizedEpsilon;
}

// Returns true if the point is in (or on) the sphere of the squared radius.
bool IsPointInSphereImpl(const glm::vec3& p, const glm::vec3& center,
                         float radius_sq) {
  return LengthSquared(p - center) <= radius_sq;
}

// Returns true if the infinite line through origin along dir passes within the
// squared radius of the center. dir must not be zero.
//
// The squared distance from the line is |to_center x dir|^2 / |dir|^2, so this
// compares without dividing.
bool IsLineThroughSphere(const glm::vec3& to_center, const glm::vec3& dir,
                         float radius_sq) {
  return LengthSquared(glm::cross(to_center, dir)) <=
         radius_sq * LengthSquared(dir);
}

// Returns true if the ray hits the sphere. dir need not be normalized.
bool IsRayInSphere(const glm::vec3& origin, const glm::vec3& dir,
                   const glm::vec3& center, float radius_sq) {
  const glm::vec3 to_center = center - origin;
  if (glm::dot(to_center, dir) <= 0.0f) {
    // The closest point on the ray is its origin (which includes a zero dir).
    return LengthSquared(to_center) <= radius_sq;
  }
  return IsLineThroughSphere(to_center, dir, radius_sq);
}

bool IsLineInSphere(const glm::vec3& l0, const glm::vec3& l1,
                    const glm::vec3& center, float radius_sq) {
  const glm::vec3 dir = l1 - l0;
  const glm::vec3 to_center = center - l0;
  const float along = glm::dot(to_center, dir);
  if (along <= 0.0f) {
    // The closest point on the segment is l0 (which includes a zero length).
    return LengthSquared(to_center) <= radius_sq;
  }
  if (along >= LengthSquared(dir)) {
    // The closest point on the segment is l1.
    return IsPointInSphereImpl(l1, center, radius_sq);
  }
  return IsLineThroughSphere(to_center, dir, radius_sq);
}

glm::vec3 SphereSupport(const CSphere& sphere, const glm::vec3& dir) {
  return sphere.center + glm::normalize(dir) * sphere.radius;
}

void SphereCcdSupport(const void* obj, const ccd_vec3_t* dir, ccd_vec3_t* vec) {
  const glm::vec3 result =
      SphereSupport(*static_cast<const CSphere*>(obj),
                    glm::vec3(dir->v[0], dir->v[1], dir->v[2]));
  vec->v[0] = result.x;
  vec->v[1] = result.y;
  vec->v[2] = result.z;
}

glm::vec3 CapsuleSupport(const CCapsule& capsule, const glm::vec3& dir) {
  if (glm::dot(capsule.vertex[0], dir) > glm::dot(capsule.vertex[1], dir)) {
    return capsule.vertex[0] + glm::normalize(dir) * capsule.radius;
  } else {
    return capsule.vertex[1] + glm::normalize(dir) * capsule.radius;
  }
}

void CapsuleCcdSupport(const void* obj, const ccd_vec3_t* dir,
                       ccd_vec3_t* vec) {
  const glm::vec3 result =
      CapsuleSupport(*static_cast<const CCapsule*>(obj),
                     glm::vec3(dir->v[0], dir->v[1], dir->v[2]));
  vec->v[0] = result.x;
  vec->v[1] = result.y;
  vec->v[2] = result.z;
}

glm::vec3 PointCloudSupport(const CPointCloud& points, const glm::vec3& dir) {
  DCHECK(!points.points.empty());
  glm::vec3 result = {};
  float max_dot = std::numeric_limits<float>::lowest();
  for (const glm::vec3& point : points.points) {
    float dot = glm::dot(point, dir);
    if (dot > max_dot) {
      result = point;
      max_dot = dot;
    }
  }
  return result;
}

void PointCloudCcdSupport(const void* obj, const ccd_vec3_t* dir,
                          ccd_vec3_t* vec) {
  const glm::vec3 result =
      PointCloudSupport(*static_cast<const CPointCloud*>(obj),
                        glm::vec3(dir->v[0], dir->v[1], dir->v[2]));
  vec->v[0] = result.x;
  vec->v[1] = result.y;
  vec->v[2] = result.z;
}

// Returns the libccd settings to collide two shapes with the support functions.
ccd_t CreateCcd(ccd_support_fn support1, ccd_support_fn support2) {
  ccd_t ccd;
  CCD_INIT(&ccd);
  ccd.support1 = support1;
  ccd.support2 = support2;
  ccd.max_iterations = 100;
  return ccd;
}

}  // namespace

bool IsPointInTriangle(const glm::vec3& p, const glm::vec3& v0,
                       const glm::vec3& v1, const glm::vec3& v2) {
  // The point is inside if it is on the inner side of each edge, which is the
  // side where the edge and the point wind the same way as the triangle.
  const glm::vec3 normal = glm::cross(v1 - v0, v2 - v0);
  return glm::dot(glm::cross(v1 - v0, p - v0), normal) >= 0.0f &&
         glm::dot(glm::cross(v2 - v1, p - v1), normal) >= 0.0f &&
         glm::dot(glm::cross(v0 - v2, p - v2), normal) >= 0.0f;
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
  return IsRayInSphere(origin, dir, center, radius * radius);
}

bool LineHitsSphere(const glm::vec3& l0, const glm::vec3& l1,
                    const glm::vec3& center, float radius) {
  return IsLineInSphere(l0, l1, center, radius * radius);
}

bool SphereHitsTriangle(const glm::vec3& center, float radius,
                        const glm::vec3& v0, const glm::vec3& v1,
                        const glm::vec3& v2, const glm::vec3& normal) {
  DCHECK(IsNormalized(normal));

  // Quick reject. If we don't intersect the plane (from either side), then we
  // don't intersect the triangle.
  const float plane_distance = glm::dot(normal, center - v0);
  if (std::abs(plane_distance) > radius) {
    return false;
  }

  const float radius_sq = radius * radius;
  return
      // If the center projects onto the triangle, then we intersect.
      IsPointInTriangle(center, v0, v1, v2) ||
      // Otherwise we intersect only if an edge (including its end points)
      // intersects the sphere.
      IsLineInSphere(v0, v1, center, radius_sq) ||
      IsLineInSphere(v0, v2, center, radius_sq) ||
      IsLineInSphere(v1, v2, center, radius_sq);
}

bool SphereHitsCapsule(const CSphere& a, const CCapsule& b) {
  const ccd_t ccd = CreateCcd(SphereCcdSupport, CapsuleCcdSupport);
  return ccdGJKIntersect(&a, &b, &ccd);
}

bool SphereHitsConvexMesh(const CSphere& a, const CPointCloud& b,
                          CollisionInfo* result) {
  const ccd_t ccd = CreateCcd(SphereCcdSupport, PointCloudCcdSupport);
  if (result == nullptr) {
    return ccdGJKIntersect(&a, &b, &ccd);
  }
  ccd_vec3_t sep;
  if (ccdGJKSeparate(&a, &b, &ccd, &sep) != 0) {
    return false;
  }
  result->penetration = {sep.v[0], sep.v[1], sep.v[2]};
  return true;
}

bool CapsuleHitsCapsule(const CCapsule& a, const CCapsule& b) {
  const ccd_t ccd = CreateCcd(CapsuleCcdSupport, CapsuleCcdSupport);
  return ccdGJKIntersect(&a, &b, &ccd);
}

bool CapsuleHitsConvexMesh(const CCapsule& a, const CPointCloud& b) {
  const ccd_t ccd = CreateCcd(CapsuleCcdSupport, PointCloudCcdSupport);
  return ccdGJKIntersect(&a, &b, &ccd);
}

}  // namespace gb
