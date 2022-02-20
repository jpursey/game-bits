// Copyright (c) 2022 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_COLLIDE_INTERSECT_H
#define GB_COLLIDE_INTERSECT_H

#include "gb/collide/shapes.h"

namespace gb {

// Epsilon value used in collision routines.
inline constexpr float kCEpsilon = 0.0000001f;

//------------------------------------------------------------------------------
// Direct intersection routines.
//
// Volume intersections return only whether the shapes in question collide, and
// may optionally support a CollisionInfo result to give further detail on the
// intersection. Ray/line intersection can also optionally provide the
// intersection (via the *At function variants).
//
// Requesting an intersection point or collision result is generally a more
// expensive operation, so should be used only when needed.
//
// Convenience "Intersects" overloads for every combination of C* shape classes
// are defined after.
//------------------------------------------------------------------------------

// Contains further information about a collision.
struct CollisionInfo {
  // Penetration depth of first shape into the second shape. Moving the first
  // shape in the opposite direction will result in the shapes just touching.
  glm::vec3 penetration;
};

//------------------------------------------------------------------------------
// Point / Triangle intersection
//------------------------------------------------------------------------------

bool IsPointInTriangle(const glm::vec3& p, const glm::vec3& v0,
                       const glm::vec3& v1, const glm::vec3& v2);

inline bool Intersects(const glm::vec3& a, CTriangle& b) {
  return IsPointInTriangle(a, b.vertex[0], b.vertex[1], b.vertex[2]);
}

//------------------------------------------------------------------------------
// Point / Sphere intersection
//------------------------------------------------------------------------------

bool IsPointInSphere(const glm::vec3& p, const glm::vec3& center, float radius);

inline bool Intersects(const glm::vec3& a, CSphere& b) {
  return IsPointInSphere(a, b.center, b.radius);
}

//------------------------------------------------------------------------------
// Ray / Triangle intersection
//------------------------------------------------------------------------------

// Intersects a ray against a triangle, returning true if an intersection
// occurs.
//
// Note: "dir" does NOT need to be normalized.
//
// If "normal" is provided, it defines the front face of the triangle, and only
// that side will be hit. "normal" must be normalized.
//
// If "t" is provided and an intersection occurs, it will be updated with the
// magnitude of "dir" where intersection occurs (it is always positive). The
// intersection point is then: origin + dir * t
//
// Alternatively if "pos" is provided and an intersection occurs, it will be
// updated with the actual intersection point.
bool RayHitsTriangleAt(const glm::vec3& origin, const glm::vec3& dir,
                       const glm::vec3& v0, const glm::vec3& v1,
                       const glm::vec3& v2, float& out_t);
inline bool RayHitsNormalTriangleAt(const glm::vec3& origin,
                                    const glm::vec3& dir, const glm::vec3& v0,
                                    const glm::vec3& v1, const glm::vec3& v2,
                                    const glm::vec3& normal, float& out_t) {
  return glm::dot(dir, normal) <= kCEpsilon &&
         RayHitsTriangleAt(origin, dir, v0, v1, v2, out_t);
}

inline bool RayHitsTriangle(const glm::vec3& origin, const glm::vec3& dir,
                            const glm::vec3& v0, const glm::vec3& v1,
                            const glm::vec3& v2) {
  float t;
  return RayHitsTriangleAt(origin, dir, v0, v1, v2, t);
}
inline bool RayHitsNormalTriangle(const glm::vec3& origin, const glm::vec3& dir,
                                  const glm::vec3& v0, const glm::vec3& v1,
                                  const glm::vec3& v2,
                                  const glm::vec3& normal) {
  float t;
  return RayHitsNormalTriangleAt(origin, dir, v0, v1, v2, normal, t);
}

inline bool RayHitsTriangleAt(const glm::vec3& origin, const glm::vec3& dir,
                              const glm::vec3& v0, const glm::vec3& v1,
                              const glm::vec3& v2, glm::vec3& out_pos) {
  float t;
  if (!RayHitsTriangleAt(origin, dir, v0, v1, v2, t)) {
    return false;
  }
  out_pos = origin + dir * t;
}
inline bool RayHitsNormalTriangleAt(const glm::vec3& origin,
                                    const glm::vec3& dir, const glm::vec3& v0,
                                    const glm::vec3& v1, const glm::vec3& v2,
                                    const glm::vec3& normal,
                                    glm::vec3& out_pos) {
  float t;
  if (!RayHitsNormalTriangleAt(origin, dir, v0, v1, v2, normal, t)) {
    return false;
  }
  out_pos = origin + dir * t;
}

inline bool Intersects(const CRay& a, const CTriangle& b) {
  return RayHitsTriangle(a.origin, a.dir, b.vertex[0], b.vertex[1],
                         b.vertex[2]);
}
inline bool Intersects(const CRay& a, const CTriangle& b, float& out_t) {
  return RayHitsTriangleAt(a.origin, a.dir, b.vertex[0], b.vertex[1],
                           b.vertex[2], out_t);
}
inline bool Intersects(const CRay& a, const CTriangle& b, glm::vec3& out_pos) {
  return RayHitsTriangleAt(a.origin, a.dir, b.vertex[0], b.vertex[1],
                           b.vertex[2], out_pos);
}
inline bool Intersects(const CRay& a, const CNormalTriangle& b) {
  return RayHitsNormalTriangle(a.origin, a.dir, b.vertex[0], b.vertex[1],
                               b.vertex[2], b.normal);
}
inline bool Intersects(const CRay& a, const CNormalTriangle& b, float& out_t) {
  return RayHitsNormalTriangleAt(a.origin, a.dir, b.vertex[0], b.vertex[1],
                                 b.vertex[2], b.normal, out_t);
}
inline bool Intersects(const CRay& a, const CNormalTriangle& b,
                       glm::vec3& out_pos) {
  return RayHitsNormalTriangleAt(a.origin, a.dir, b.vertex[0], b.vertex[1],
                                 b.vertex[2], b.normal, out_pos);
}

//------------------------------------------------------------------------------
// Line / Triangle intersection
//------------------------------------------------------------------------------

// Intersects a line segment against a triangle, returning true if an
// intersection occurs.
//
// If "normal" is provided, it defines the front face of the triangle, and only
// that side will be hit. "normal" must be normalized.
//
// If "pos" is provided and an intersection occurs, it will be updated with the
// actual intersection point.
inline bool LineHitsTriangle(const glm::vec3& l0, const glm::vec3& l1,
                             const glm::vec3& v0, const glm::vec3& v1,
                             const glm::vec3& v2) {
  float t;
  return RayHitsTriangleAt(l0, l1 - l0, v0, v1, v2, t) && t <= 1.0f;
}
inline bool LineHitsNormalTriangle(const glm::vec3& l0, const glm::vec3& l1,
                                   const glm::vec3& v0, const glm::vec3& v1,
                                   const glm::vec3& v2,
                                   const glm::vec3& normal) {
  float t;
  return RayHitsNormalTriangleAt(l0, l1 - l0, v0, v1, v2, normal, t) &&
         t <= 1.0f;
}

inline bool LineHitsTriangleAt(const glm::vec3& l0, const glm::vec3& l1,
                               const glm::vec3& v0, const glm::vec3& v1,
                               const glm::vec3& v2, glm::vec3& out_pos) {
  const glm::vec3 dir = l1 - l0;
  float t;
  if (!RayHitsTriangleAt(l0, dir, v0, v1, v2, t) || t > 1.0f) {
    return false;
  }
  out_pos = l0 + dir * t;
  return true;
}
inline bool LineHitsNormalTriangleAt(const glm::vec3& l0, const glm::vec3& l1,
                                     const glm::vec3& v0, const glm::vec3& v1,
                                     const glm::vec3& v2,
                                     const glm::vec3& normal,
                                     glm::vec3& out_pos) {
  const glm::vec3 dir = l1 - l0;
  float t;
  if (!RayHitsNormalTriangleAt(l0, dir, v0, v1, v2, normal, t) || t > 1.0f) {
    return false;
  }
  out_pos = l0 + dir * t;
  return true;
}

inline bool Intersects(const CLine& a, const CTriangle& b) {
  return LineHitsTriangle(a.vertex[0], a.vertex[1], b.vertex[0], b.vertex[1],
                          b.vertex[2]);
}
inline bool Intersects(const CLine& a, const CTriangle& b, glm::vec3& out_pos) {
  return LineHitsTriangleAt(a.vertex[0], a.vertex[1], b.vertex[0], b.vertex[1],
                            b.vertex[2], out_pos);
}
inline bool Intersects(const CTriangle& a, const CLine& b) {
  return Intersects(b, a);
}
inline bool Intersects(const CTriangle& a, const CLine& b, glm::vec3& out_pos) {
  return Intersects(b, a, out_pos);
}

inline bool Intersects(const CLine& a, const CNormalTriangle& b) {
  return LineHitsNormalTriangle(a.vertex[0], a.vertex[1], b.vertex[0],
                                b.vertex[1], b.vertex[2], b.normal);
}
inline bool Intersects(const CLine& a, const CNormalTriangle& b,
                       glm::vec3& out_pos) {
  return LineHitsNormalTriangleAt(a.vertex[0], a.vertex[1], b.vertex[0],
                                  b.vertex[1], b.vertex[2], b.normal, out_pos);
}
inline bool Intersects(const CNormalTriangle& a, const CLine& b) {
  return Intersects(b, a);
}
inline bool Intersects(const CNormalTriangle& a, const CLine& b,
                       glm::vec3& out_pos) {
  return Intersects(b, a, out_pos);
}

//------------------------------------------------------------------------------
// Ray / Sphere intersection
//------------------------------------------------------------------------------

// Intersects a ray against a sphere, returning true if an intersection occurs.
//
// If "i" is provided and an intersection occurs, it will be updated with the
// actual intersection point.
//
// Note: if "i" is not required, then "dir" does NOT need to be normalized.
// Otherwise, it must be.
bool RayHitsSphere(const glm::vec3& origin, const glm::vec3& dir,
                   const glm::vec3& center, float radius);

inline bool Intersects(const CRay& a, const CSphere& b) {
  return RayHitsSphere(a.origin, a.dir, b.center, b.radius);
}

//------------------------------------------------------------------------------
// Line / Sphere intersection
//------------------------------------------------------------------------------

// Intersects a line segment against a triangle, returning true if an
// intersection occurs.
//
// If "i" is provided and an intersection occurs, it will be updated with the
// actual intersection point.
bool LineHitsSphere(const glm::vec3& l0, const glm::vec3& l1,
                    const glm::vec3& center, float radius);

inline bool Intersects(const CLine& a, const CSphere& b) {
  return LineHitsSphere(a.vertex[0], a.vertex[1], b.center, b.radius);
}
inline bool Intersects(const CSphere& a, const CLine& b) {
  return Intersects(b, a);
}

//------------------------------------------------------------------------------
// Sphere / Triangle intersection
//------------------------------------------------------------------------------

// Intersects a sphere a triangle, returning true if they intersect.
//
// A normal for the triangle may also be provided (this must be normalized) if
// it is known as an optimization.
bool SphereHitsTriangle(const glm::vec3& center, float radius,
                        const glm::vec3& v0, const glm::vec3& v1,
                        const glm::vec3& v2, const glm::vec3& normal);
inline bool SphereHitsTriangle(const glm::vec3& center, float radius,
                               const glm::vec3& v0, const glm::vec3& v1,
                               const glm::vec3& v2) {
  return SphereHitsTriangle(center, radius, v0, v1, v2,
                            GetTriangleNormal(v0, v1, v2));
}

inline bool Intersects(const CSphere& a, const CTriangle& b) {
  return SphereHitsTriangle(a.center, a.radius, b.vertex[0], b.vertex[1],
                            b.vertex[2]);
}
inline bool Intersects(const CTriangle& a, const CSphere& b) {
  return SphereHitsTriangle(b.center, b.radius, a.vertex[0], a.vertex[1],
                            a.vertex[2]);
}
inline bool Intersects(const CSphere& a, const CNormalTriangle& b) {
  return SphereHitsTriangle(a.center, a.radius, b.vertex[0], b.vertex[1],
                            b.vertex[2], b.normal);
}
inline bool Intersects(const CNormalTriangle& a, const CSphere& b) {
  return SphereHitsTriangle(b.center, b.radius, a.vertex[0], a.vertex[1],
                            a.vertex[2], a.normal);
}

//------------------------------------------------------------------------------
// Sphere / Sphere intersection
//------------------------------------------------------------------------------

// Intersects a sphere with another sphere, returning true if they intersect.
inline bool SphereHitsSphere(const glm::vec3& a_center, float a_radius,
                             const glm::vec3& b_center, float b_radius) {
  const glm::vec3 delta = a_center - b_center;
  return glm::dot(delta, delta) <= a_radius * b_radius;
}

inline bool Intersects(const CSphere& a, const CSphere& b) {
  return SphereHitsSphere(a.center, a.radius, b.center, b.radius);
}

//------------------------------------------------------------------------------
// Sphere / Capsule intersection
//------------------------------------------------------------------------------

bool SphereHitsCapsule(const CSphere& a, const CCapsule& b);
inline bool SphereHitsCapsule(const glm::vec3& a_center, float a_radius,
                              const glm::vec3& b_v1, const glm::vec3& b_v2,
                              float b_radius) {
  return SphereHitsCapsule(CSphere(a_center, a_radius),
                           CCapsule(b_v1, b_v2, b_radius));
}

inline bool Intersects(const CSphere& a, const CCapsule& b) {
  return SphereHitsCapsule(a, b);
}
inline bool Intersects(const CCapsule& a, const CSphere& b) {
  return SphereHitsCapsule(b, a);
}

//------------------------------------------------------------------------------
// Sphere / Convex mesh intersection
//------------------------------------------------------------------------------

bool SphereHitsConvexMesh(const CSphere& a, const CPointCloud& b,
                          CollisionInfo* result = nullptr);
inline bool SphereHitsConvexMesh(const glm::vec3& a_center, float a_radius,
                                 const glm::vec3* b_points, int b_point_count,
                                 CollisionInfo* result = nullptr) {
  return SphereHitsConvexMesh(CSphere(a_center, a_radius),
                              CPointCloud(b_points, b_point_count), result);
}

inline bool Intersects(const CSphere& a, const CPointCloud& b,
                       CollisionInfo* result = nullptr) {
  return SphereHitsConvexMesh(a, b, result);
}

//------------------------------------------------------------------------------
// Capsule / Capsule intersection
//------------------------------------------------------------------------------

bool CapsuleHitsCapsule(const CCapsule& a, const CCapsule& b);
inline bool CapsuleHitsCapsule(const glm::vec3& a_v1, const glm::vec3& a_v2,
                               float a_radius, const glm::vec3& b_v1,
                               const glm::vec3& b_v2, float b_radius) {
  return CapsuleHitsCapsule(CCapsule(a_v1, a_v2, a_radius),
                            CCapsule(b_v1, b_v2, b_radius));
}

inline bool Intersects(const CCapsule& a, const CCapsule& b) {
  return CapsuleHitsCapsule(a, b);
}

//------------------------------------------------------------------------------
// Capsule / Convex mesh intersection
//------------------------------------------------------------------------------

bool CapsuleHitsConvexMesh(const CCapsule& a, const CPointCloud& b);
inline bool CapsuleHitsConvexMesh(const glm::vec3& a_v1, const glm::vec3& a_v2,
                                  float a_radius, const glm::vec3* b_points,
                                  int b_point_count) {
  return CapsuleHitsConvexMesh(CCapsule(a_v1, a_v2, a_radius),
                               CPointCloud(b_points, b_point_count));
}

inline bool Intersects(const CCapsule& a, const CPointCloud& b) {
  return CapsuleHitsConvexMesh(a, b);
}

//------------------------------------------------------------------------------
// Aabb / Aabb intersection
//------------------------------------------------------------------------------

// Intersects an axis-aligned bounding box (AABB) with another AABB, returning
// true if they intersect.
//
// The bounding box size must not be negative.
inline bool AabbHitsAabb(const glm::vec3& a_pos, const glm::vec3& a_size,
                         const glm::vec3& b_pos, const glm::vec3& b_size) {
  return a_pos.x + a_size.x > b_pos.x && b_pos.x + b_size.x > a_pos.x &&
         a_pos.y + a_size.y > b_pos.y && b_pos.y + b_size.y > a_pos.y &&
         a_pos.z + a_size.z > b_pos.z && b_pos.z + b_size.z > a_pos.z;
}

inline bool Intersects(const CAabb& a, const CAabb& b) {
  return AabbHitsAabb(a.pos, a.size, b.pos, b.size);
}

}  // namespace gb

#endif  // GB_COLLIDE_INTERSECT_H
