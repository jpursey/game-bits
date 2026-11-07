#ifndef WORLD_H_
#define WORLD_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gb/base/validated_context.h"
#include "gb/pilot/pilot.h"
#include "gb/render/binding_data.h"
#include "gb/render/mesh_pool.h"
#include "gb/render/render_types.h"
#include "gb/render/storage_buffer.h"
#include "gb/resource/resource_set.h"
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

  // OPTIONAL: Pilot. With one, Draw() builds the mesh of every chunk in view
  // in the frame it is first drawn, rather than only for part of each frame,
  // so what is drawn doesn't depend on how fast the program runs.
  static GB_CONTEXT_CONSTRAINT(kConstraintPilot, kInOptional, gb::Pilot);

  using Contract =
      gb::ContextContract<kConstraintRenderSystem, kConstraintWorldResources,
                          kConstraintPilot>;

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

  gb::RenderView* GetView() const { return view_.get(); }

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

  // Sets whether every chunk in view is drawn from one mesh pool, in one
  // indirect draw, or each chunk is drawn on its own. Chunks are drawn from
  // the pool by default.
  //
  // This takes effect from the next Draw(), which rebuilds the mesh of each
  // chunk it draws. Until then, IsDrawingFromPool() returns the current way.
  void SetDrawFromPool(bool draw_from_pool) {
    next_draw_from_pool_ = draw_from_pool;
  }
  bool IsDrawingFromPool() const { return draw_from_pool_; }

  // Returns the pool that chunk meshes are created in while the world draws
  // from it.
  gb::MeshPool* GetChunkPool() const { return chunk_pool_; }

  void Draw(const Camera& camera);
  void DrawLightingGui();

 private:
  World(gb::ValidatedContext context);

  bool InitGraphics();

  // Creates the buffers of draws and positions for `count` chunks drawn from
  // the pool, replacing any there were.
  bool CreatePoolBuffers(int count);

  // Draws the chunks added to `pool_draws_` and `chunk_positions_` this frame,
  // in one indirect draw.
  void DrawFromPool();

  std::unique_ptr<Chunk> NewChunk(const ChunkIndex& index);
  void InitFlatWorldChunk(Chunk* chunk);
  void InitSineWorldChunk(Chunk* chunk);
  void InitPerlinWorldChunk(Chunk* chunk);

  gb::ValidatedContext context_;

  // The world's render resources, which include the chunk pool, so they are
  // declared before the chunks, whose meshes must be destroyed first.
  gb::ResourceSet resources_;

  // Game data
  absl::flat_hash_map<std::tuple<int, int>, std::unique_ptr<Chunk>> chunks_;

  // Render data
  bool use_frustum_cull_ = true;
  bool use_cull_camera_ = false;
  Camera cull_camera_;
  std::unique_ptr<gb::RenderView> view_;
  gb::Material* material_ = nullptr;
  ViewLightData lights_;
  gb::Pixel sky_color_;

  // Drawing from the pool. Each frame, a chunk drawn from the pool adds its
  // draw to `pool_draws_`, with its draw index as its first instance, and its
  // position to `chunk_positions_` at that index, which DrawFromPool() writes
  // to the buffers. Chunk meshes are built for `draw_from_pool_`, which
  // Draw() sets from `next_draw_from_pool_`.
  bool draw_from_pool_ = true;
  bool next_draw_from_pool_ = true;
  gb::MeshPool* chunk_pool_ = nullptr;
  gb::Material* pool_material_ = nullptr;
  std::unique_ptr<gb::BindingData> pool_instance_data_;
  gb::StorageBuffer* pool_draw_buffer_ = nullptr;
  gb::StorageBuffer* chunk_position_buffer_ = nullptr;
  std::vector<gb::MeshDraw> pool_draws_;
  std::vector<ChunkPosition> chunk_positions_;
};

#endif  // WORLD_H_
