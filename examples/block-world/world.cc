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

  return true;
}

Chunk* World::GetChunk(const glm::ivec3& index) {
  auto& chunk = chunks_[{index.x, index.y, index.z}];
  if (chunk == nullptr) {
    chunk = NewChunk(index);
  }
  return chunk.get();
}

std::unique_ptr<Chunk> World::NewChunk(const glm::ivec3& index) {
  auto chunk = std::make_unique<Chunk>(this, index);
  InitSineWorldChunk(chunk.get());
  chunk->Update();
  return chunk;
}

void World::InitFlatWorldChunk(Chunk* chunk) {
  glm::ivec3 index = chunk->GetIndex();
  if (index.y > 0) {
    return;
  }
  for (int x = 0; x < Chunk::kSize.x; ++x) {
    for (int y = 0; y < Chunk::kSize.y; ++y) {
      for (int z = 0; z < Chunk::kSize.z; ++z) {
        if (y < 4 || index.y < 0) {
          chunk->Set(x, y, z, kBlockRock2);
        } else if (y < 8) {
          chunk->Set(x, y, z, kBlockRock1);
        } else if (y < 12) {
          chunk->Set(x, y, z, kBlockDirt);
        } else if (y == 12) {
          chunk->Set(x, y, z, kBlockGrass);
        }
      }
    }
  }
}

void World::InitSineWorldChunk(Chunk* chunk) {
  InitFlatWorldChunk(chunk);
  glm::ivec3 index = chunk->GetIndex();
  if (index.y != 0) {
    return;
  }
  static constexpr float kSineScale = 0.1f;
  for (int x = 0; x < Chunk::kSize.x; ++x) {
    for (int z = 0; z < Chunk::kSize.z; ++z) {
      const float v = std::sin((index.x * Chunk::kSize.x + x) * kSineScale) +
                      std::sin((index.z * Chunk::kSize.z + z) * kSineScale);
      const int depth = std::clamp(static_cast<int>(v * 3.0f) + 8, 0, 13);
      for (int y = depth; y < 13; ++y) {
        chunk->Set(x, y, z, kBlockAir);
      }
    }
  }
}

void World::Draw(const Camera& camera) {
  const glm::vec3 world_position = camera.GetPosition();
  const float distance = camera.GetViewDistance();
  glm::ivec3 chunk_position(
      static_cast<int>(world_position.x / Chunk::kSize.x),
      static_cast<int>(world_position.y / Chunk::kSize.y),
      static_cast<int>(world_position.z / Chunk::kSize.z));
  const glm::ivec3 chunk_distance(static_cast<int>(distance / Chunk::kSize.x),
                                  static_cast<int>(distance / Chunk::kSize.y),
                                  static_cast<int>(distance / Chunk::kSize.z));
  glm::ivec3 chunk_min_position = chunk_position - chunk_distance;
  glm::ivec3 chunk_max_position = chunk_position + chunk_distance;

  // Clamp vertical distance
  if (chunk_min_position.y > kMaxChunkIndexY ||
      chunk_max_position.y < kMinChunkIndexY) {
    return;
  }
  if (chunk_min_position.y < kMinChunkIndexY) {
    chunk_min_position.y = kMinChunkIndexY;
  }
  if (chunk_max_position.y > kMaxChunkIndexY) {
    chunk_max_position.y = kMaxChunkIndexY;
  }

  // Hack to only render y = 0;
  chunk_position.y = chunk_min_position.y = chunk_max_position.y = 0;

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
                       0.1f, distance * 2);
  projection[1][1] *= -1.0f;
  scene_->GetSceneBindingData()->SetConstants(
      0, SceneData{projection * camera.GetView()});

  // Start in the current chunk and then flood fill to all visible chunks.
  absl::flat_hash_set<std::tuple<int, int, int>> visited;
  std::vector<glm::ivec3> chunk_queue;
  // This is rediculously innefficient, and will be better with culling
  chunk_queue.reserve(16000);
  chunk_queue.push_back(chunk_position);
  int queue_index = 0;
  while (queue_index < static_cast<int>(chunk_queue.size())) {
    auto index = chunk_queue[queue_index];
    if (!visited.insert({index.x, index.y, index.z}).second) {
      ++queue_index;
      continue;
    }
    Chunk* chunk = GetChunk(chunk_queue[queue_index++]);
    if (!chunk->HasMesh()) {
      chunk->BuildMesh();
    }
    auto instance_data = chunk->GetInstanceData();
    auto meshes = chunk->GetMesh();
    if (instance_data != nullptr && !meshes.empty()) {
      for (gb::Mesh* mesh : meshes) {
        render_system->Draw(scene_.get(), mesh, instance_data);
      }
    }

    // TODO: Traverse to neighboring mesh that is not culled.

    if (index.x > chunk_min_position.x) {
      chunk_queue.push_back({index.x - 1, index.y, index.z});
    }
    if (index.x < chunk_max_position.x) {
      chunk_queue.push_back({index.x + 1, index.y, index.z});
    }
    if (index.y > chunk_min_position.y) {
      chunk_queue.push_back({index.x, index.y - 1, index.z});
    }
    if (index.y < chunk_max_position.y) {
      chunk_queue.push_back({index.x, index.y + 1, index.z});
    }
    if (index.z > chunk_min_position.z) {
      chunk_queue.push_back({index.x, index.y, index.z - 1});
    }
    if (index.z < chunk_max_position.z) {
      chunk_queue.push_back({index.x, index.y, index.z + 1});
    }
  }
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
  ImGui::End();
}