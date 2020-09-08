#ifndef CUBE_H_
#define CUBE_H_

#include "gb/render/render_types.h"
#include "glm/glm.hpp"

// Side index
inline constexpr int kCubePx = 0;
inline constexpr int kCubeNx = 1;
inline constexpr int kCubePy = 2;
inline constexpr int kCubeNy = 3;
inline constexpr int kCubePz = 4;
inline constexpr int kCubeNz = 5;

// Corner index
inline constexpr int kCubeNxNyNz = 0;
inline constexpr int kCubePxNyNz = 1;
inline constexpr int kCubePxPyNz = 2;
inline constexpr int kCubeNxPyNz = 3;
inline constexpr int kCubeNxNyPz = 4;
inline constexpr int kCubePxNyPz = 5;
inline constexpr int kCubePxPyPz = 6;
inline constexpr int kCubeNxPyPz = 7;

// The eight corners of a cube with the lower front left corner at the origin:
//   kCubeNxNyNz, kCubePxNyNz, kCubePxPyNz, kCubeNxPyNz,
//   kCubeNxNyPz, kCubePxNyPz, kCubePxPyPz, kCubeNxPyPz
inline constexpr glm::vec3 kCubePosition[8] = {
    {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
    {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
};

// Normals for the cube size:
//   kCubePx, kCubeNx, kCubePy, kCubeNy, kCubePz, kCubeNz
inline constexpr glm::vec3 kCubeSideNormal[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

// Vertex index info kCubePosition for each side in clockwise order:
//   kCubePx, kCubeNx, kCubePy, kCubeNy, kCubePz, kCubeNz
inline constexpr uint16_t kCubeSideVertex[6][4] = {
    {5, 1, 2, 6}, {0, 4, 7, 3}, {3, 7, 6, 2},
    {0, 1, 5, 4}, {4, 5, 6, 7}, {1, 0, 3, 2},
};

// Texture coordinates for each side.
inline constexpr glm::vec2 kCubeSideUv[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};

// Ordering of the kCubeSideVertex into two counterclockwise triangles.
inline constexpr gb::Triangle kCubeSideTriangle[2] = {{0, 1, 2}, {0, 2, 3}};

#endif  // CUBE_H_
