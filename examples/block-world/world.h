#ifndef WORLD_H_
#define WORLD_H_

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "gb/base/validated_context.h"
#include "gb/render/render_types.h"
#include "glm/glm.hpp"

// Game includes
#include "camera.h"
#include "chunk.h"
#include "game_types.h"
#include "scene_types.h"

struct HitInfo {
  glm::ivec3 index;  // World index.
  int face;          // Cube face that was hit.
  BlockId block;     // Block type hit.
};

// The world class contains the state of the entire game world
class World final {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: RenderSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintRenderSystem, kInRequired,
                               gb::RenderSystem);

  // REQUIRED: WorldResources.
  static GB_CONTEXT_CONSTRAINT(kConstraintWorldResources, kInRequired,
                               WorldResources);

  using Contract =
      gb::ContextContract<kConstraintRenderSystem, kConstraintWorldResources>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  World(const World&) = delete;
  World(World&&) = delete;
  World& operator=(const World&) = delete;
  World& operator=(World&&) = delete;
  ~World();

  static std::unique_ptr<World> Create(Contract contract);

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  const gb::ValidatedContext& GetContext() const { return context_; }

  gb::RenderScene* GetScene() const { return scene_.get(); }

  //----------------------------------------------------------------------------
  // Chunk access
  //----------------------------------------------------------------------------

  // Returns the chunk index for the specified world position.
  //
  // If specified, "block_index" will receive the index of the block within the
  // chunk.
  bool GetIndex(int x, int y, int z, ChunkIndex* chunk_index,
                glm::ivec3* block_index);
  bool GetIndex(const glm::ivec3& position, ChunkIndex* chunk_index,
                glm::ivec3* block_index) {
    return GetIndex(position.x, position.y, position.z, chunk_index,
                    block_index);
  }
  bool GetIndex(const glm::vec3& position, ChunkIndex* chunk_index,
                glm::ivec3* block_index) {
    return GetIndex(static_cast<int>(std::floor(position.x)),
                    static_cast<int>(std::floor(position.y)),
                    static_cast<int>(std::floor(position.z)), chunk_index,
                    block_index);
  }

  // Returns the chunk for the specified
  Chunk* GetChunk(const ChunkIndex& index);

  // Returns the block at the requested position.
  BlockId GetBlock(int x, int y, int z);
  BlockId GetBlock(const glm::ivec3& index) {
    return GetBlock(index.x, index.y, index.z);
  }
  BlockId GetBlock(const glm::vec3& position) {
    return GetBlock(static_cast<int>(std::floor(position.x)),
                    static_cast<int>(std::floor(position.y)),
                    static_cast<int>(std::floor(position.z)));
  }

  // Set a block at the requested position.
  void SetBlock(int x, int y, int z, BlockId block);
  void SetBlock(const glm::ivec3& index, BlockId block) {
    SetBlock(index.x, index.y, index.z, block);
  }
  void SetBlock(const glm::vec3& position, BlockId block) {
    SetBlock(static_cast<int>(std::floor(position.x)),
             static_cast<int>(std::floor(position.y)),
             static_cast<int>(std::floor(position.z)), block);
  }

  // Casts a ray until a block is hit and returns the results.
  HitInfo RayCast(const glm::vec3& position, const glm::vec3& ray,
                  float distance);

  //----------------------------------------------------------------------------
  // Rendering
  //----------------------------------------------------------------------------

  void EnableCullCamera(const Camera& camera) {
    cull_camera_ = camera;
    use_cull_camera_ = true;
  }
  void DisableCullCamera() { use_cull_camera_ = false; }

  void Draw(const Camera& camera);
  void DrawLightingGui();

 private:
  World(gb::ValidatedContext context);

  bool InitGraphics();

  std::unique_ptr<Chunk> NewChunk(const ChunkIndex& index);
  void InitFlatWorldChunk(Chunk* chunk);
  void InitSineWorldChunk(Chunk* chunk);
  void InitPerlinWorldChunk(Chunk* chunk);

  gb::ValidatedContext context_;

  // Game data
  absl::flat_hash_map<std::tuple<int, int>, std::unique_ptr<Chunk>> chunks_;

  // Render data
  bool use_frustum_cull_ = true;
  bool use_cull_camera_ = false;
  Camera cull_camera_;
  std::unique_ptr<gb::RenderScene> scene_;
  gb::Material* material_ = nullptr;
  SceneLightData lights_;
  gb::Pixel sky_color_;
};

#endif  // WORLD_H_
