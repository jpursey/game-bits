// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/collide/shapes.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(ShapesTest, TriangleNormalIsCounterclockwise) {
  EXPECT_EQ(GetTriangleNormal({0, 0, 0}, {1, 0, 0}, {0, 1, 0}),
            glm::vec3(0, 0, 1));
  EXPECT_EQ(GetTriangleNormal({0, 0, 0}, {0, 1, 0}, {1, 0, 0}),
            glm::vec3(0, 0, -1));
  EXPECT_EQ(GetTriangleNormal({5, 5, 5}, {5, 7, 5}, {5, 5, 9}),
            glm::vec3(1, 0, 0));
}

TEST(ShapesTest, Bounds) {
  const glm::vec3 a(1, -2, 3);
  const glm::vec3 b(-1, 2, 0);
  const glm::vec3 c(0, 0, 5);
  EXPECT_EQ(GetMinBounds(a, b), glm::vec3(-1, -2, 0));
  EXPECT_EQ(GetMaxBounds(a, b), glm::vec3(1, 2, 3));
  EXPECT_EQ(GetMinBounds(a, b, c), glm::vec3(-1, -2, 0));
  EXPECT_EQ(GetMaxBounds(a, b, c), glm::vec3(1, 2, 5));
}

TEST(ShapesTest, NormalTriangle) {
  const CNormalTriangle computed({0, 0, 0}, {1, 0, 0}, {0, 1, 0});
  EXPECT_EQ(computed.normal, glm::vec3(0, 0, 1));
  const CNormalTriangle from_triangle(
      CTriangle({0, 0, 0}, {0, 1, 0}, {1, 0, 0}));
  EXPECT_EQ(from_triangle.normal, glm::vec3(0, 0, -1));
  const CNormalTriangle given({0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 1, 0});
  EXPECT_EQ(given.normal, glm::vec3(0, 1, 0));
}

TEST(ShapesTest, ShapesFromArrays) {
  const glm::vec3 vertices[] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
  const CLine line(vertices);
  EXPECT_EQ(line.vertex[1], vertices[1]);
  const CTriangle triangle(vertices);
  EXPECT_EQ(triangle.vertex[2], vertices[2]);
  const CCapsule capsule(vertices, 2.0f);
  EXPECT_EQ(capsule.vertex[1], vertices[1]);
  EXPECT_EQ(capsule.radius, 2.0f);
  const CPointCloud points(vertices, 3);
  EXPECT_EQ(points.points.size(), 3);
}

TEST(ShapesTest, AabbFromLine) {
  const CAabb aabb(CLine({1, -2, 3}, {-1, 2, 0}));
  EXPECT_EQ(aabb.pos, glm::vec3(-1, -2, 0));
  EXPECT_EQ(aabb.size, glm::vec3(2, 4, 3));
}

TEST(ShapesTest, AabbFromTriangle) {
  const CAabb aabb(CTriangle({1, -2, 3}, {-1, 2, 0}, {0, 0, 5}));
  EXPECT_EQ(aabb.pos, glm::vec3(-1, -2, 0));
  EXPECT_EQ(aabb.size, glm::vec3(2, 4, 5));
}

TEST(ShapesTest, AabbFromSphere) {
  const CAabb aabb(CSphere({1, 2, 3}, 2.0f));
  EXPECT_EQ(aabb.pos, glm::vec3(-1, 0, 1));
  EXPECT_EQ(aabb.size, glm::vec3(4, 4, 4));
}

}  // namespace
}  // namespace gb
