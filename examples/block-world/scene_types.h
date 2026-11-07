#ifndef SCENE_TYPES_H_
#define SCENE_TYPES_H_

#include "gb/image/pixel.h"
#include "gb/render/render_types.h"
#include "glm/glm.hpp"

struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec3 uv;
  gb::Pixel color;
};

struct ViewData {
  glm::mat4 view_projection;
};

struct ViewLightData {
  glm::vec4 ambient;    // w is intensity
  glm::vec4 sun_color;  // w is intensity
  glm::vec3 sun_direction;
};

struct InstanceData {
  glm::mat4 model;
};

// Matches ChunkPosition in block_pool.vert, which reads a storage buffer of
// them at this binding in the view's binding set.
struct ChunkPosition {
  glm::vec4 offset;  // xyz is added to each vertex.
};
inline constexpr int kChunkPositionsBinding = 2;

inline constexpr glm::vec3 kUpAxis = glm::vec3(0, 0, 1);

#endif  // SCENE_TYPES_H_
