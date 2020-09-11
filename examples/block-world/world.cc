#include "world.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "gb/render/render_system.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "imgui.h"

// Game includes
#include "block.h"
#include "camera.h"
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
          glm::vec3(0.15f, -0.4f, -0.8f)),  // Down at a convenient angle
  };
  scene_->GetSceneBindingData()->SetConstants(1, lights_);
  sky_color_ = gb::Pixel(69, 136, 221);
  render_system->SetClearColor(sky_color_);

  return true;
}

Chunk* World::GetChunk(const ChunkIndex& index) {
  auto& chunk = chunks_[{index.x, index.z}];
  if (chunk == nullptr) {
    chunk = NewChunk(index);
  }
  return chunk.get();
}

std::unique_ptr<Chunk> World::NewChunk(const ChunkIndex& index) {
  auto chunk = std::make_unique<Chunk>(this, index);
  InitSineWorldChunk(chunk.get());
  chunk->Update();
  return chunk;
}

void World::InitFlatWorldChunk(Chunk* chunk) {
  ChunkIndex index = chunk->GetIndex();
  for (int x = 0; x < Chunk::kSize.x; ++x) {
    for (int y = 0; y < Chunk::kSize.y; ++y) {
      for (int z = 0; z < Chunk::kSize.z; ++z) {
        if (y < 100) {
          chunk->Set(x, y, z, kBlockRock2);
        } else if (y < 108) {
          chunk->Set(x, y, z, kBlockRock1);
        } else if (y < 116) {
          chunk->Set(x, y, z, kBlockDirt);
        } else if (y == 116) {
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
    for (int z = 0; z < Chunk::kSize.z; ++z) {
      const float v = std::sin((index.x * Chunk::kSize.x + x) * kSineScale) +
                      std::sin((index.z * Chunk::kSize.z + z) * kSineScale);
      const int depth = std::clamp(static_cast<int>(v * 6.0f) + 108, 0, 117);
      for (int y = depth; y < 117; ++y) {
        chunk->Set(x, y, z, kBlockAir);
      }
    }
  }
}

void World::Draw(const Camera& camera) {
  const Camera cull_camera = (use_cull_camera_ ? cull_camera_ : camera);
  const glm::vec3 cull_position = cull_camera.GetPosition();
  const float cull_distance = cull_camera.GetViewDistance();

  ChunkIndex chunk_position = {
      static_cast<int>(cull_position.x / Chunk::kSize.x),
      static_cast<int>(cull_position.z / Chunk::kSize.z)};
  // Negative positions floor toward zero, which puts them in the wrong chunk
  // (off by one).
  if (cull_position.x < 0) {
    --chunk_position.x;
  }
  if (cull_position.z < 0) {
    --chunk_position.z;
  }
  const ChunkIndex chunk_distance = {
      static_cast<int>(cull_distance / Chunk::kSize.x),
      static_cast<int>(cull_distance / Chunk::kSize.z)};
  const ChunkIndex chunk_min_position = {chunk_position.x - chunk_distance.x,
                                         chunk_position.z - chunk_distance.z};
  const ChunkIndex chunk_max_position = {chunk_position.x + chunk_distance.x,
                                         chunk_position.z + chunk_distance.z};

  auto* render_system = context_.GetPtr<gb::RenderSystem>();
  auto frame_size = render_system->GetFrameDimensions();
  if (frame_size.width == 0) {
    frame_size.width = 1;
  }
  if (frame_size.height == 0) {
    frame_size.height = 1;
  }
  glm::mat4 projection =
      glm::perspective(glm::radians(45.0f),
                       frame_size.width / static_cast<float>(frame_size.height),
                       0.1f, camera.GetViewDistance() * 2);
  projection[1][1] *= -1.0f;
  scene_->GetSceneBindingData()->SetConstants(
      0, SceneData{projection * camera.GetView()});

  glm::mat4 cull_projection;
  if (!use_cull_camera_) {
    cull_projection = projection;
  } else {
    cull_projection = glm::perspective(
        glm::radians(45.0f),
        frame_size.width / static_cast<float>(frame_size.height), 0.1f,
        cull_camera.GetViewDistance() * 2);
    cull_projection[1][1] *= -1.0f;
  }
  glm::mat4 cull_view_projection = cull_projection * cull_camera.GetView();
  FrustumNormals frustum(cull_view_projection);
  frustum.Normalize();

  int debug_triangle_count = 0;
  int debug_chunk_count = 0;

  // Start in the current chunk and then flood fill to all visible chunks.
  absl::flat_hash_set<std::tuple<int, int>> visited;
  std::vector<ChunkIndex> chunk_queue;
  chunk_queue.reserve(1000);
  chunk_queue.push_back(chunk_position);
  int queue_index = 0;
  while (queue_index < static_cast<int>(chunk_queue.size())) {
    auto index = chunk_queue[queue_index];
    if (!visited.insert({index.x, index.z}).second) {
      ++queue_index;
      continue;
    }

    Chunk* chunk = GetChunk(chunk_queue[queue_index++]);

    if (use_frustum_cull_) {
      const glm::vec3 chunk_pos = {index.x * Chunk::kSize.x, 0,
                                   index.z * Chunk::kSize.z};
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
      chunk->BuildMesh();
    }
    auto instance_data = chunk->GetInstanceData();
    auto meshes = chunk->GetMesh();
    if (instance_data != nullptr && !meshes.empty()) {
      for (gb::Mesh* mesh : meshes) {
        debug_triangle_count += mesh->GetTriangleCount();
        render_system->Draw(scene_.get(), mesh, instance_data);
      }
    }

    // TODO: Traverse to neighboring mesh that is not culled.

    if (index.x > chunk_min_position.x) {
      chunk_queue.push_back({index.x - 1, index.z});
    }
    if (index.x < chunk_max_position.x) {
      chunk_queue.push_back({index.x + 1, index.z});
    }
    if (index.z > chunk_min_position.z) {
      chunk_queue.push_back({index.x, index.z - 1});
    }
    if (index.z < chunk_max_position.z) {
      chunk_queue.push_back({index.x, index.z + 1});
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