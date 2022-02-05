#include "world.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "gb/render/render_system.h"
#include "imgui.h"
#include "stb_perlin.h"

// Game includes
#include "block.h"
#include "camera.h"
#include "cube.h"
#include "world_resources.h"

namespace {

inline constexpr glm::vec3 kChunkVertices[8] = {
    glm::vec3(0, 0, 0),
    glm::vec3(Chunk::kSize.x, 0, 0),
    glm::vec3(0, Chunk::kSize.y, 0),
    glm::vec3(Chunk::kSize.x, Chunk::kSize.y, 0),
    glm::vec3(0, 0, Chunk::kSize.z),
    glm::vec3(Chunk::kSize.x, 0, Chunk::kSize.z),
    glm::vec3(0, Chunk::kSize.y, Chunk::kSize.z),
    glm::vec3(Chunk::kSize.x, Chunk::kSize.y, Chunk::kSize.z),
};

inline bool IsChunkOutsidePlane(const glm::vec3& chunk_pos,
                                const glm::vec3& plane_pos,
                                const glm::vec3& normal) {
  for (int i = 0; i < 8; ++i) {
    if (glm::dot(chunk_pos + kChunkVertices[i] - plane_pos, normal) >= 0) {
      return false;
    }
  }
  return true;
}

struct FrustumNormals {
  FrustumNormals(const glm::mat4& view_projection) {
    for (int i = 0; i < 3; ++i) {
      const float v = view_projection[i][3];
      left[i] = v + view_projection[i][0];
      right[i] = v - view_projection[i][0];
      bottom[i] = v + view_projection[i][1];
      top[i] = v - view_projection[i][1];
      near[i] = v + view_projection[i][2];
      far[i] = v - view_projection[i][2];
    }
  }
  void Normalize() {
    left = glm::normalize(left);
    right = glm::normalize(right);
    top = glm::normalize(top);
    bottom = glm::normalize(bottom);
    near = glm::normalize(near);
    far = glm::normalize(far);
  }

  glm::vec3 left;
  glm::vec3 right;
  glm::vec3 top;
  glm::vec3 bottom;
  glm::vec3 near;
  glm::vec3 far;
};

}  // namespace

World::World(gb::ValidatedContext context) : context_(std::move(context)) {}

World::~World() {}

std::unique_ptr<World> World::Create(Contract contract) {
  if (!contract.IsValid()) {
    return nullptr;
  }
  auto world = absl::WrapUnique(new World(std::move(contract)));
  if (!world->InitGraphics()) {
    return nullptr;
  }
  return world;
}

bool World::InitGraphics() {
  auto* render_system = context_.GetPtr<gb::RenderSystem>();
  auto* resources = context_.GetPtr<WorldResources>();

  material_ = resources->GetChunkMaterial();

  scene_ = render_system->CreateScene(resources->GetSceneType());
  if (scene_ == nullptr) {
    LOG(ERROR) << "Could not create scene";
    return false;
  }

  lights_ = {
      // Ambient
      {1.0f, 1.0f, 1.0f, 0.1f},  // White light at 10% brightness

      // Sun
      {1.0f, 0.91f, 0.655f, 1.0f},  // Light yellow light, full bright
      glm::normalize(
          glm::vec3(0.15f, -0.8f, -0.4f)),  // Down at a convenient angle
  };
  scene_->GetSceneBindingData()->SetConstants(1, lights_);
  sky_color_ = gb::Pixel(69, 136, 221);
  render_system->SetClearColor(sky_color_);

  return true;
}

std::unique_ptr<Chunk> World::NewChunk(const ChunkIndex& index) {
  auto chunk = std::make_unique<Chunk>(this, index);
  InitPerlinWorldChunk(chunk.get());
  chunk->Update();
  return chunk;
}

void World::InitFlatWorldChunk(Chunk* chunk) {
  for (int x = 0; x < Chunk::kSize.x; ++x) {
    for (int y = 0; y < Chunk::kSize.y; ++y) {
      for (int z = 0; z < Chunk::kSize.z; ++z) {
        if (z < 100) {
          chunk->Set(x, y, z, kBlockRock2);
        } else if (z < 108) {
          chunk->Set(x, y, z, kBlockRock1);
        } else if (z < 116) {
          chunk->Set(x, y, z, kBlockDirt);
        } else if (z == 116) {
          chunk->Set(x, y, z, kBlockGrass);
        }
      }
    }
  }
}

void World::InitSineWorldChunk(Chunk* chunk) {
  InitFlatWorldChunk(chunk);
  ChunkIndex index = chunk->GetIndex();
  static constexpr float kSineScale = 0.1f;
  for (int x = 0; x < Chunk::kSize.x; ++x) {
    for (int y = 0; y < Chunk::kSize.y; ++y) {
      const float v = std::sin((index.x * Chunk::kSize.x + x) * kSineScale) +
                      std::sin((index.y * Chunk::kSize.y + y) * kSineScale);
      const int depth = std::clamp(static_cast<int>(v * 6.0f) + 108, 0, 117);
      for (int z = depth; z < 117; ++z) {
        chunk->Set(x, y, z, kBlockAir);
      }
    }
  }
}

void World::InitPerlinWorldChunk(Chunk* chunk) {
  ChunkIndex index = chunk->GetIndex();
  static float kHorizontalScale = 0.008f;
  static float kVerticalScale = static_cast<float>(Chunk::kSize.z / 4);
  for (int x = 0; x < Chunk::kSize.x; ++x) {
    for (int y = 0; y < Chunk::kSize.y; ++y) {
      const float perlin_x = (index.x * Chunk::kSize.x + x) * kHorizontalScale;
      const float perlin_y = (index.y * Chunk::kSize.y + y) * kHorizontalScale;
      const float v = stb_perlin_ridge_noise3(perlin_x, perlin_y, 0.0f, 2.0f,
                                              0.5f, 0.8f, 4);
      const int max_height = Chunk::kSize.z * 3 / 4;
      const int height =
          std::clamp(static_cast<int>(v * kVerticalScale) + Chunk::kSize.z / 3,
                     0, max_height);
      int z = 0;
      for (; z < height; ++z) {
        if (z < 80) {
          chunk->Set(x, y, z, kBlockRock2);
        } else if (z < 90) {
          chunk->Set(x, y, z, kBlockGrass);
        } else if (z < 100) {
          chunk->Set(x, y, z, kBlockDirt);
        } else {
          chunk->Set(x, y, z, kBlockRock1);
        }
      }
    }
  }
}

bool World::GetIndex(int x, int y, int z, ChunkIndex* chunk_index,
                     glm::ivec3* block_index) {
  if (z < 0 || z > Chunk::kSize.z) {
    *chunk_index = {0, 0};
    if (block_index != nullptr) {
      *block_index = {0, 0, 0};
    }
    return false;
  }
  chunk_index->x = x / Chunk::kSize.x;
  x %= Chunk::kSize.x;
  if (x < 0) {
    x += Chunk::kSize.x;
    --chunk_index->x;
  }

  chunk_index->y = y / Chunk::kSize.y;
  y %= Chunk::kSize.y;
  if (y < 0) {
    y += Chunk::kSize.y;
    --chunk_index->y;
  }

  if (block_index != nullptr) {
    *block_index = {x, y, z};
  }
  return true;
}

Chunk* World::GetChunk(const ChunkIndex& index) {
  auto& chunk = chunks_[{index.x, index.y}];
  if (chunk == nullptr) {
    chunk = NewChunk(index);
  }
  return chunk.get();
}

BlockId World::GetBlock(int x, int y, int z) {
  ChunkIndex chunk_index;
  glm::ivec3 block_index;
  if (!GetIndex(x, y, z, &chunk_index, &block_index)) {
    return kBlockAir;
  }
  return GetChunk(chunk_index)->Get(block_index);
}

void World::SetBlock(int x, int y, int z, BlockId block) {
  ChunkIndex chunk_index;
  glm::ivec3 block_index;
  if (!GetIndex(x, y, z, &chunk_index, &block_index)) {
    return;
  }
  Chunk* chunk = GetChunk(chunk_index);
  bool had_block = (chunk->Get(block_index) != kBlockAir);
  chunk->Set(block_index, block);
  if (had_block != (block != kBlockAir)) {
    if (block_index.x == 0) {
      GetChunk({chunk_index.x - 1, chunk_index.y})->Invalidate();
    } else if (block_index.x == Chunk::kSize.x - 1) {
      GetChunk({chunk_index.x + 1, chunk_index.y})->Invalidate();
    }
    if (block_index.y == 0) {
      GetChunk({chunk_index.x, chunk_index.y - 1})->Invalidate();
    } else if (block_index.y == Chunk::kSize.y - 1) {
      GetChunk({chunk_index.x, chunk_index.y + 1})->Invalidate();
    }
  }
}

HitInfo World::RayCast(const glm::vec3& position, const glm::vec3& ray,
                       float distance) {
  static constexpr float kMinAxisValue = 0.000001f;  // Treated as zero.

  glm::ivec3 world_index = {std::floor(position.x), std::floor(position.y),
                            std::floor(position.z)};
  HitInfo hit = {world_index, 0, kBlockAir};

  ChunkIndex chunk_index;
  glm::ivec3 block_index;
  if (!GetIndex(position, &chunk_index, &block_index)) {
    return hit;
  }
  Chunk* chunk = GetChunk(chunk_index);

  const glm::ivec3 step = {
      ray.x < 0 ? -1 : 1,
      ray.y < 0 ? -1 : 1,
      ray.z < 0 ? -1 : 1,
  };
  const glm::vec3 delta = {
      (std::fabs(ray.x) < kMinAxisValue ? 1.0f / kMinAxisValue
                                        : 1.0f / std::fabs(ray.x)),
      (std::fabs(ray.y) < kMinAxisValue ? 1.0f / kMinAxisValue
                                        : 1.0f / std::fabs(ray.y)),
      (std::fabs(ray.z) < kMinAxisValue ? 1.0f / kMinAxisValue
                                        : 1.0f / std::fabs(ray.z)),
  };
  glm::vec3 max = {
      (world_index.x - position.x + (ray.x < 0 ? 0.0f : 1.0f)) / ray.x,
      (world_index.y - position.y + (ray.y < 0 ? 0.0f : 1.0f)) / ray.y,
      (world_index.z - position.z + (ray.z < 0 ? 0.0f : 1.0f)) / ray.z,
  };

  // Sides that are crossed from the perspective of the new block.
  const int sides[3] = {
      step.x < 0 ? kCubePx : kCubeNx,
      step.y < 0 ? kCubePy : kCubeNy,
      step.z < 0 ? kCubePz : kCubeNz,
  };
  int crossed_side = sides[0];

  glm::vec3 travel = {0, 0, 0};
  const float distance_squared = distance * distance;

  while (true) {
    if (max.x < max.y) {
      if (max.x < max.z) {
        crossed_side = sides[0];
        block_index.x += step.x;
        world_index.x += step.x;
        max.x += delta.x;
        travel.x += 1.0f;
        if (block_index.x < 0) {
          block_index.x = Chunk::kSize.x - 1;
          --chunk_index.x;
          chunk = GetChunk(chunk_index);
        } else if (block_index.x >= Chunk::kSize.x) {
          block_index.x = 0;
          ++chunk_index.x;
          chunk = GetChunk(chunk_index);
        }
      } else {
        crossed_side = sides[2];
        block_index.z += step.z;
        world_index.z += step.z;
        max.z += delta.z;
        travel.z += 1.0f;
        if (block_index.z < 0 || block_index.z >= Chunk::kSize.z) {
          hit.index = world_index;
          hit.face = crossed_side;
          return hit;
        }
      }
    } else {
      if (max.y < max.z) {
        crossed_side = sides[1];
        block_index.y += step.y;
        world_index.y += step.y;
        max.y += delta.y;
        travel.y += 1.0f;
        if (block_index.y < 0) {
          block_index.y = Chunk::kSize.y - 1;
          --chunk_index.y;
          chunk = GetChunk(chunk_index);
        } else if (block_index.y >= Chunk::kSize.y) {
          block_index.y = 0;
          ++chunk_index.y;
          chunk = GetChunk(chunk_index);
        }
      } else {
        crossed_side = sides[2];
        block_index.z += step.z;
        world_index.z += step.z;
        max.z += delta.z;
        travel.z += 1.0f;
        if (block_index.z < 0 || block_index.z >= Chunk::kSize.z) {
          hit.index = world_index;
          hit.face = crossed_side;
          return hit;
        }
      }
    }
    if (glm::dot(travel, travel) > distance_squared) {
      hit.index = world_index;
      hit.face = crossed_side;
      return hit;
    }
    hit.block = chunk->Get(block_index);
    if (hit.block != kBlockAir) {
      break;
    }
  }
  hit.index = world_index;
  hit.face = crossed_side;
  return hit;
}

void World::Draw(const Camera& camera) {
  const Camera cull_camera = (use_cull_camera_ ? cull_camera_ : camera);
  const glm::vec3 cull_position = cull_camera.GetPosition();
  const float cull_distance = cull_camera.GetViewDistance();

  ChunkIndex chunk_index;
  if (!GetIndex(cull_position, &chunk_index, nullptr)) {
    return;
  }
  const ChunkIndex chunk_distance = {
      static_cast<int>(cull_distance / Chunk::kSize.x),
      static_cast<int>(cull_distance / Chunk::kSize.y)};
  const ChunkIndex chunk_min_position = {chunk_index.x - chunk_distance.x,
                                         chunk_index.y - chunk_distance.y};
  const ChunkIndex chunk_max_position = {chunk_index.x + chunk_distance.x,
                                         chunk_index.y + chunk_distance.y};

  auto* render_system = context_.GetPtr<gb::RenderSystem>();
  glm::mat4 projection =
      camera.CreateProjection(render_system->GetFrameDimensions());
  scene_->GetSceneBindingData()->SetConstants(
      0, SceneData{projection * camera.GetView()});

  glm::mat4 cull_projection;
  if (!use_cull_camera_) {
    cull_projection = projection;
  } else {
    cull_projection =
        cull_camera.CreateProjection(render_system->GetFrameDimensions());
  }
  glm::mat4 cull_view_projection = cull_projection * cull_camera.GetView();
  FrustumNormals frustum(cull_view_projection);
  frustum.Normalize();

  int debug_triangle_count = 0;
  int debug_chunk_count = 0;

  static constexpr int64_t kMaxTime = 10LL * 1000 * 1000;  // 10 milliseconds
  int64_t start_time = absl::GetCurrentTimeNanos();

  absl::flat_hash_set<std::tuple<int, int>> visited;
  std::vector<ChunkIndex> chunk_queue;
  chunk_queue.reserve(1000);
  chunk_queue.push_back(chunk_index);
  int queue_index = 0;
  while (queue_index < static_cast<int>(chunk_queue.size())) {
    auto index = chunk_queue[queue_index];
    if (!visited.insert({index.x, index.y}).second) {
      ++queue_index;
      continue;
    }

    Chunk* chunk = GetChunk(chunk_queue[queue_index++]);

    if (use_frustum_cull_) {
      const glm::vec3 chunk_pos = {index.x * Chunk::kSize.x,
                                   index.y * Chunk::kSize.y, 0};
      if (IsChunkOutsidePlane(chunk_pos, cull_position + frustum.near * 0.1f,
                              frustum.near) ||
          IsChunkOutsidePlane(chunk_pos, cull_position, frustum.left) ||
          IsChunkOutsidePlane(chunk_pos, cull_position, frustum.right) ||
          IsChunkOutsidePlane(chunk_pos, cull_position, frustum.top) ||
          IsChunkOutsidePlane(chunk_pos, cull_position, frustum.bottom)) {
        continue;
      }
    }

    ++debug_chunk_count;
    if (!chunk->HasMesh()) {
      if (absl::GetCurrentTimeNanos() - start_time < kMaxTime) {
        chunk->BuildMesh();
      }
    }
    if (chunk->HasMesh()) {
      auto instance_data = chunk->GetInstanceData();
      auto meshes = chunk->GetMesh();
      if (instance_data != nullptr && !meshes.empty()) {
        for (gb::Mesh* mesh : meshes) {
          debug_triangle_count += mesh->GetTriangleCount();
          render_system->Draw(scene_.get(), mesh, material_, instance_data);
        }
      }
    }
    if (index.x > chunk_min_position.x) {
      chunk_queue.push_back({index.x - 1, index.y});
    }
    if (index.x < chunk_max_position.x) {
      chunk_queue.push_back({index.x + 1, index.y});
    }
    if (index.y > chunk_min_position.y) {
      chunk_queue.push_back({index.x, index.y - 1});
    }
    if (index.y < chunk_max_position.y) {
      chunk_queue.push_back({index.x, index.y + 1});
    }
  }

  ImGui::Begin("World Render Stats");
  if (ImGui::Checkbox("Freeze cull camera", &use_cull_camera_)) {
    if (use_cull_camera_) {
      EnableCullCamera(camera);
    } else {
      DisableCullCamera();
    }
  }
  ImGui::Checkbox("Frustum culling", &use_frustum_cull_);
  ImGui::Text("Chunks: %d", debug_chunk_count);
  ImGui::Text("Triangles: %d", debug_triangle_count);
  ImGui::End();
}

void World::DrawLightingGui() {
  ImGui::Begin("Lighting");
  bool modified = false;
  modified = ImGui::ColorEdit3("Ambient Color", &lights_.ambient.r) || modified;
  modified = ImGui::DragFloat("Ambient Brightness", &lights_.ambient.a, 0.001f,
                              0.0f, 1.0f) ||
             modified;
  modified = ImGui::ColorEdit3("Sun Color", &lights_.sun_color.r) || modified;
  modified = ImGui::DragFloat("Sun Brightness", &lights_.sun_color.a, 0.001f,
                              0.0f, 1.0f) ||
             modified;
  modified = ImGui::DragFloat3("Sun Direction", &lights_.sun_direction.x, 0.01f,
                               -1.0f, 1.0f) ||
             modified;
  if (modified) {
    scene_->GetSceneBindingData()->SetConstants(1, lights_);
  }
  float sky_color[3] = {static_cast<float>(sky_color_.r) / 255.0f,
                        static_cast<float>(sky_color_.g) / 255.0f,
                        static_cast<float>(sky_color_.b) / 255.0f};
  if (ImGui::ColorEdit3("Sky Color", sky_color)) {
    sky_color_.r =
        static_cast<uint8_t>(std::clamp(sky_color[0] * 255.0f, 0.0f, 255.0f));
    sky_color_.g =
        static_cast<uint8_t>(std::clamp(sky_color[1] * 255.0f, 0.0f, 255.0f));
    sky_color_.b =
        static_cast<uint8_t>(std::clamp(sky_color[2] * 255.0f, 0.0f, 255.0f));
    context_.GetPtr<gb::RenderSystem>()->SetClearColor(sky_color_);
  }
  ImGui::End();
}
