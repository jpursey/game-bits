// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/collide/intersect.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

// A triangle in the z=0 plane, facing +z.
const CNormalTriangle kTriangle({0, 0, 0}, {2, 0, 0}, {0, 2, 0});

// The same triangle without a normal, so rays and lines hit it from both sides.
const CTriangle& kTwoSidedTriangle = kTriangle;

// The points of a box covering [0.5, 2.5] in x, and [-1, 1] in y and z.
const glm::vec3 kBox[] = {
    {0.5f, -1, -1}, {2.5f, -1, -1}, {0.5f, 1, -1}, {2.5f, 1, -1},
    {0.5f, -1, 1},  {2.5f, -1, 1},  {0.5f, 1, 1},  {2.5f, 1, 1},
};

//==============================================================================
// Point intersection
//==============================================================================

TEST(IntersectTest, PointInTriangle) {
  EXPECT_TRUE(IsPointInTriangle({0.5f, 0.5f, 0}, kTriangle.vertex[0],
                                kTriangle.vertex[1], kTriangle.vertex[2]));
  EXPECT_TRUE(IsPointInTriangle({1, 0, 0}, kTriangle.vertex[0],
                                kTriangle.vertex[1], kTriangle.vertex[2]));
  EXPECT_TRUE(IsPointInTriangle({0, 0, 0}, kTriangle.vertex[0],
                                kTriangle.vertex[1], kTriangle.vertex[2]));
  EXPECT_FALSE(IsPointInTriangle({1.5f, 1.5f, 0}, kTriangle.vertex[0],
                                 kTriangle.vertex[1], kTriangle.vertex[2]));
  EXPECT_FALSE(IsPointInTriangle({-0.5f, 0.5f, 0}, kTriangle.vertex[0],
                                 kTriangle.vertex[1], kTriangle.vertex[2]));
}

TEST(IntersectTest, PointInTriangleIsProjected) {
  EXPECT_TRUE(IsPointInTriangle({0.5f, 0.5f, 3}, kTriangle.vertex[0],
                                kTriangle.vertex[1], kTriangle.vertex[2]));
  EXPECT_TRUE(IsPointInTriangle({0.5f, 0.5f, -3}, kTriangle.vertex[0],
                                kTriangle.vertex[1], kTriangle.vertex[2]));
  EXPECT_FALSE(IsPointInTriangle({1.5f, 1.5f, 3}, kTriangle.vertex[0],
                                 kTriangle.vertex[1], kTriangle.vertex[2]));
}

TEST(IntersectTest, PointBeyondTriangleEdgeIsOutside) {
  // On the line through each edge, past either end.
  const glm::vec3 kOutside[] = {{3, 0, 0},  {-1, 0, 0}, {0, 3, 0},
                                {0, -1, 0}, {3, -1, 0}, {-1, 3, 0}};
  for (const glm::vec3& p : kOutside) {
    EXPECT_FALSE(IsPointInTriangle(p, kTriangle.vertex[0], kTriangle.vertex[1],
                                   kTriangle.vertex[2]))
        << p.x << ", " << p.y;
  }
}

TEST(IntersectTest, PointInConstShapes) {
  EXPECT_TRUE(Intersects(glm::vec3(0.5f, 0.5f, 0), kTriangle));
  const CSphere sphere({1, 1, 1}, 1.0f);
  EXPECT_TRUE(Intersects(glm::vec3(1.5f, 1, 1), sphere));
  EXPECT_FALSE(Intersects(glm::vec3(2.5f, 1, 1), sphere));
}

TEST(IntersectTest, PointInSphere) {
  EXPECT_TRUE(IsPointInSphere({1, 1, 1}, {1, 1, 1}, 1.0f));
  EXPECT_TRUE(IsPointInSphere({1.9f, 1, 1}, {1, 1, 1}, 1.0f));
  EXPECT_FALSE(IsPointInSphere({2.1f, 1, 1}, {1, 1, 1}, 1.0f));
}

//==============================================================================
// Ray / Triangle
//==============================================================================

TEST(IntersectTest, RayHitsTriangle) {
  float t = 0;
  EXPECT_TRUE(RayHitsTriangleAt({0.5f, 0.5f, 4}, {0, 0, -2},
                                kTriangle.vertex[0], kTriangle.vertex[1],
                                kTriangle.vertex[2], t));
  EXPECT_FLOAT_EQ(t, 2.0f);

  // Hits from behind too.
  EXPECT_TRUE(RayHitsTriangle({0.5f, 0.5f, -4}, {0, 0, 1}, kTriangle.vertex[0],
                              kTriangle.vertex[1], kTriangle.vertex[2]));

  // Pointing away, beside, and parallel.
  EXPECT_FALSE(RayHitsTriangle({0.5f, 0.5f, 4}, {0, 0, 1}, kTriangle.vertex[0],
                               kTriangle.vertex[1], kTriangle.vertex[2]));
  EXPECT_FALSE(RayHitsTriangle({1.5f, 1.5f, 4}, {0, 0, -1}, kTriangle.vertex[0],
                               kTriangle.vertex[1], kTriangle.vertex[2]));
  EXPECT_FALSE(RayHitsTriangle({-1, 0.5f, 0}, {1, 0, 0}, kTriangle.vertex[0],
                               kTriangle.vertex[1], kTriangle.vertex[2]));
}

TEST(IntersectTest, RayHitsTriangleAtPos) {
  glm::vec3 pos(0, 0, 0);
  EXPECT_TRUE(RayHitsTriangleAt({0.5f, 0.25f, 4}, {0, 0, -2},
                                kTriangle.vertex[0], kTriangle.vertex[1],
                                kTriangle.vertex[2], pos));
  EXPECT_EQ(pos, glm::vec3(0.5f, 0.25f, 0));
  EXPECT_TRUE(RayHitsNormalTriangleAt(
      {0.5f, 0.25f, 4}, {0, 0, -2}, kTriangle.vertex[0], kTriangle.vertex[1],
      kTriangle.vertex[2], kTriangle.normal, pos));
  EXPECT_EQ(pos, glm::vec3(0.5f, 0.25f, 0));
  EXPECT_FALSE(RayHitsTriangleAt({1.5f, 1.5f, 4}, {0, 0, -2},
                                 kTriangle.vertex[0], kTriangle.vertex[1],
                                 kTriangle.vertex[2], pos));
}

TEST(IntersectTest, RayHitsNormalTriangleFrontOnly) {
  EXPECT_TRUE(Intersects(CRay({0.5f, 0.5f, 4}, {0, 0, -1}), kTriangle));
  EXPECT_FALSE(Intersects(CRay({0.5f, 0.5f, -4}, {0, 0, 1}), kTriangle));
  float t = 0;
  EXPECT_TRUE(Intersects(CRay({0.5f, 0.5f, 4}, {0, 0, -1}), kTriangle, t));
  EXPECT_FLOAT_EQ(t, 4.0f);
  glm::vec3 pos(0, 0, 0);
  EXPECT_TRUE(
      Intersects(CRay({0.5f, 0.5f, 4}, {0, 0, -1}), kTwoSidedTriangle, pos));
  EXPECT_EQ(pos, glm::vec3(0.5f, 0.5f, 0));
}

//==============================================================================
// Line / Triangle
//==============================================================================

TEST(IntersectTest, LineHitsTriangle) {
  EXPECT_TRUE(Intersects(CLine({0.5f, 0.5f, 1}, {0.5f, 0.5f, -1}), kTriangle));
  EXPECT_FALSE(Intersects(CLine({0.5f, 0.5f, -1}, {0.5f, 0.5f, 1}), kTriangle));
  EXPECT_TRUE(
      Intersects(CLine({0.5f, 0.5f, -1}, {0.5f, 0.5f, 1}), kTwoSidedTriangle));

  // Ends before reaching the triangle.
  EXPECT_FALSE(
      Intersects(CLine({0.5f, 0.5f, 2}, {0.5f, 0.5f, 1}), kTwoSidedTriangle));

  glm::vec3 pos(0, 0, 0);
  EXPECT_TRUE(
      Intersects(CLine({0.5f, 0.25f, 1}, {0.5f, 0.25f, -3}), kTriangle, pos));
  EXPECT_EQ(pos, glm::vec3(0.5f, 0.25f, 0));
}

//==============================================================================
// Ray and Line / Sphere
//==============================================================================

TEST(IntersectTest, RayHitsSphere) {
  const CSphere sphere({0, 0, 0}, 1.0f);
  EXPECT_TRUE(Intersects(CRay({-10, 0.5f, 0}, {1, 0, 0}), sphere));
  EXPECT_FALSE(Intersects(CRay({-10, 1.5f, 0}, {1, 0, 0}), sphere));
  EXPECT_FALSE(Intersects(CRay({10, 0.5f, 0}, {1, 0, 0}), sphere));
  EXPECT_TRUE(Intersects(CRay({0.5f, 0, 0}, {1, 0, 0}), sphere));
}

TEST(IntersectTest, RayHitsSphereWithUnnormalizedDir) {
  const CSphere sphere({0, 0, 0}, 1.0f);
  EXPECT_TRUE(Intersects(CRay({-10, 0.5f, 0}, {3, 0, 0}), sphere));
  EXPECT_TRUE(Intersects(CRay({-10, -10, 0}, {0.1f, 0.1f, 0}), sphere));
  EXPECT_FALSE(Intersects(CRay({-10, 0, 0}, {2, 2, 0}), sphere));
}

TEST(IntersectTest, LineHitsSphere) {
  const CSphere sphere({0, 0, 0}, 1.0f);

  // Through the sphere.
  EXPECT_TRUE(Intersects(CLine({-3, 0, 0}, {3, 0, 0}), sphere));
  EXPECT_TRUE(Intersects(sphere, CLine({-3, 0.5f, 0}, {3, 0.5f, 0})));

  // Ending in the sphere, starting in the sphere, and inside it.
  EXPECT_TRUE(Intersects(CLine({-3, 0, 0}, {-0.5f, 0, 0}), sphere));
  EXPECT_TRUE(Intersects(CLine({0.5f, 0, 0}, {3, 0, 0}), sphere));
  EXPECT_TRUE(Intersects(CLine({-0.1f, 0, 0}, {0.1f, 0, 0}), sphere));

  // Ending before the sphere, starting after it, and passing beside it.
  EXPECT_FALSE(Intersects(CLine({-3, 0, 0}, {-1.5f, 0, 0}), sphere));
  EXPECT_FALSE(Intersects(CLine({1.5f, 0, 0}, {3, 0, 0}), sphere));
  EXPECT_FALSE(Intersects(CLine({-3, 1.5f, 0}, {3, 1.5f, 0}), sphere));
}

//==============================================================================
// Sphere / Triangle
//==============================================================================

TEST(IntersectTest, SphereHitsTriangleFace) {
  EXPECT_TRUE(Intersects(CSphere({0.5f, 0.5f, 0.9f}, 1.0f), kTriangle));
  EXPECT_TRUE(Intersects(CSphere({0.5f, 0.5f, -0.9f}, 1.0f), kTriangle));
  EXPECT_TRUE(Intersects(kTriangle, CSphere({0.5f, 0.5f, 0}, 0.1f)));
  EXPECT_FALSE(Intersects(CSphere({0.5f, 0.5f, 1.1f}, 1.0f), kTriangle));
}

TEST(IntersectTest, SphereBehindTriangleMisses) {
  EXPECT_FALSE(Intersects(CSphere({0.5f, 0.5f, -5}, 1.0f), kTriangle));
  EXPECT_FALSE(
      Intersects(CSphere({0.5f, 0.5f, -1.1f}, 1.0f), kTwoSidedTriangle));
}

TEST(IntersectTest, SphereHitsTriangleEdgeAndCorner) {
  // Beside the hypotenuse, from (2, 0) to (0, 2).
  EXPECT_TRUE(Intersects(CSphere({1.5f, 1.5f, 0}, 0.75f), kTriangle));
  EXPECT_FALSE(Intersects(CSphere({1.5f, 1.5f, 0}, 0.65f), kTriangle));

  // Beyond the corner at (2, 0).
  EXPECT_TRUE(Intersects(CSphere({2.5f, -0.5f, 0.5f}, 1.0f), kTriangle));
  EXPECT_FALSE(Intersects(CSphere({3, -1, 1}, 1.0f), kTriangle));
}

TEST(IntersectTest, SphereHitsTriangleWithComputedNormals) {
  const CTriangle kTriangles[] = {
      {{0, 0, 0}, {3, 1, 0.7f}, {-0.3f, 2, 1.1f}},
      {{1.1f, 2.3f, -0.4f}, {-2.7f, 0.3f, 5.1f}, {0.9f, -1.3f, 2.2f}},
      {{0.1f, 0.2f, 0.3f}, {7.3f, -0.6f, 1.9f}, {2.2f, 3.3f, -4.4f}},
      {{-5, 1, 1}, {1, 1, -5}, {1, -5, 1}},
  };
  for (const CTriangle& triangle : kTriangles) {
    const glm::vec3 center =
        (triangle.vertex[0] + triangle.vertex[1] + triangle.vertex[2]) / 3.0f;
    EXPECT_TRUE(Intersects(CSphere(center, 0.1f), triangle));
    EXPECT_TRUE(Intersects(CSphere(center, 0.1f), CNormalTriangle(triangle)));
  }
}

//==============================================================================
// Sphere / Sphere
//==============================================================================

TEST(IntersectTest, SphereHitsSphere) {
  EXPECT_TRUE(Intersects(CSphere({0, 0, 0}, 1.0f), CSphere({1.5f, 0, 0}, 1)));
  EXPECT_TRUE(
      Intersects(CSphere({0, 0, 0}, 0.5f), CSphere({0.9f, 0, 0}, 0.5f)));
  EXPECT_TRUE(Intersects(CSphere({0, 0, 0}, 3.0f), CSphere({0, 1, 0}, 0.1f)));
  EXPECT_FALSE(
      Intersects(CSphere({0, 0, 0}, 1.0f), CSphere({0, 2.5f, 0}, 1.0f)));
  EXPECT_FALSE(
      Intersects(CSphere({0, 0, 0}, 0.5f), CSphere({1.1f, 0, 0}, 0.5f)));
}

//==============================================================================
// Capsules and convex meshes
//==============================================================================

TEST(IntersectTest, SphereHitsCapsule) {
  const CCapsule capsule({0, 0, 0}, {0, 4, 0}, 1.0f);
  EXPECT_TRUE(Intersects(CSphere({1.5f, 2, 0}, 1.0f), capsule));
  EXPECT_TRUE(Intersects(capsule, CSphere({0, 5.5f, 0}, 1.0f)));
  EXPECT_FALSE(Intersects(CSphere({2.5f, 2, 0}, 1.0f), capsule));
  EXPECT_FALSE(Intersects(CSphere({0, 6.5f, 0}, 1.0f), capsule));
}

TEST(IntersectTest, CapsuleHitsCapsule) {
  const CCapsule capsule({0, 0, 0}, {0, 4, 0}, 1.0f);
  EXPECT_TRUE(Intersects(capsule, CCapsule({1.5f, 2, -3}, {1.5f, 2, 3}, 1.0f)));
  EXPECT_FALSE(
      Intersects(capsule, CCapsule({2.5f, 2, -3}, {2.5f, 2, 3}, 1.0f)));
}

TEST(IntersectTest, SphereHitsConvexMesh) {
  const CPointCloud box(kBox);
  EXPECT_TRUE(Intersects(CSphere({0, 0, 0}, 1.0f), box));
  EXPECT_FALSE(Intersects(CSphere({-0.6f, 0, 0}, 1.0f), box));
  EXPECT_FALSE(Intersects(CSphere({0, 3, 0}, 1.0f), box));
}

TEST(IntersectTest, SphereHitsConvexMeshPenetration) {
  const CPointCloud box(kBox);
  CollisionInfo info;
  ASSERT_TRUE(Intersects(CSphere({0, 0, 0}, 1.0f), box, &info));
  EXPECT_NEAR(info.penetration.x, 0.5f, 0.01f);
  EXPECT_NEAR(info.penetration.y, 0.0f, 0.01f);
  EXPECT_NEAR(info.penetration.z, 0.0f, 0.01f);
  EXPECT_FALSE(Intersects(CSphere({-0.6f, 0, 0}, 1.0f), box, &info));
}

TEST(IntersectTest, CapsuleHitsConvexMesh) {
  const CPointCloud box(kBox);
  EXPECT_TRUE(Intersects(CCapsule({0, -3, 0}, {0, 3, 0}, 1.0f), box));
  EXPECT_FALSE(Intersects(CCapsule({-0.6f, -3, 0}, {-0.6f, 3, 0}, 1.0f), box));
}

//==============================================================================
// Aabb / Aabb
//==============================================================================

TEST(IntersectTest, AabbHitsAabb) {
  const CAabb aabb({0, 0, 0}, {2, 2, 2});
  EXPECT_TRUE(Intersects(aabb, CAabb({1, 1, 1}, {2, 2, 2})));
  EXPECT_TRUE(Intersects(aabb, CAabb({0.5f, 0.5f, 0.5f}, {1, 1, 1})));
  EXPECT_FALSE(Intersects(aabb, CAabb({2.5f, 0, 0}, {1, 1, 1})));
  EXPECT_FALSE(Intersects(aabb, CAabb({0, -3, 0}, {1, 2, 1})));
  EXPECT_FALSE(Intersects(aabb, CAabb({0, 0, 2.5f}, {1, 1, 1})));
}

//==============================================================================
// Touching shapes intersect
//==============================================================================

TEST(IntersectTest, TouchingPointsIntersect) {
  EXPECT_TRUE(IsPointInSphere({2, 1, 1}, {1, 1, 1}, 1.0f));
  EXPECT_TRUE(Intersects(glm::vec3(2, 0, 0), kTriangle));
}

TEST(IntersectTest, TouchingRaysAndLinesIntersect) {
  const CSphere sphere({0, 0, 0}, 1.0f);
  EXPECT_TRUE(Intersects(CRay({-10, 1, 0}, {1, 0, 0}), sphere));
  EXPECT_TRUE(Intersects(CRay({1, 0, 0}, {1, 0, 0}), sphere));
  EXPECT_TRUE(Intersects(CLine({-3, 1, 0}, {3, 1, 0}), sphere));
  EXPECT_TRUE(Intersects(CLine({-3, 0, 0}, {-1, 0, 0}), sphere));
  EXPECT_TRUE(Intersects(CLine({1, 0, 0}, {3, 0, 0}), sphere));

  // Through an edge and a corner of a triangle.
  EXPECT_TRUE(Intersects(CRay({1, 0, 1}, {0, 0, -1}), kTriangle));
  EXPECT_TRUE(Intersects(CRay({0, 0, 1}, {0, 0, -1}), kTriangle));
  EXPECT_TRUE(Intersects(CLine({1, 1, 1}, {1, 1, 0}), kTriangle));
}

TEST(IntersectTest, RayStartingOnTriangleMisses) {
  EXPECT_FALSE(Intersects(CRay({0.5f, 0.5f, 0}, {0, 0, -1}), kTriangle));
  EXPECT_FALSE(
      Intersects(CLine({0.5f, 0.5f, 0}, {0.5f, 0.5f, -1}), kTwoSidedTriangle));
}

TEST(IntersectTest, TouchingSpheresIntersect) {
  EXPECT_TRUE(Intersects(CSphere({0, 0, 0}, 1.0f), CSphere({2, 0, 0}, 1.0f)));

  // The triangle's face, edge, and corner.
  EXPECT_TRUE(Intersects(CSphere({0.5f, 0.5f, 1}, 1.0f), kTriangle));
  EXPECT_TRUE(Intersects(CSphere({0.5f, 0.5f, -1}, 1.0f), kTriangle));
  EXPECT_TRUE(Intersects(CSphere({1, -1, 0}, 1.0f), kTriangle));
  EXPECT_TRUE(Intersects(CSphere({-1, 0, 0}, 1.0f), kTriangle));
}

TEST(IntersectTest, TouchingAabbsIntersect) {
  const CAabb aabb({0, 0, 0}, {2, 2, 2});
  EXPECT_TRUE(Intersects(aabb, CAabb({2, 0, 0}, {1, 1, 1})));
  EXPECT_TRUE(Intersects(aabb, CAabb({0, -1, 0}, {1, 1, 1})));
  EXPECT_TRUE(Intersects(aabb, CAabb({2, 2, 2}, {1, 1, 1})));
}

}  // namespace
}  // namespace gb
