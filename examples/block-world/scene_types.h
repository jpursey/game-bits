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

struct SceneData {
  glm::mat4 view_projection;
};

struct SceneLightData {
  glm::vec4 ambient;    // w is intensity
  glm::vec4 sun_color;  // w is intensity
  glm::vec3 sun_direction;
};

struct InstanceData {
  glm::mat4 model;
};

inline constexpr glm::vec3 kUpAxis = glm::vec3(0, 0, 1);

#endif  // SCENE_TYPES_H_
