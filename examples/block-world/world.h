#ifndef WORLD_H_
#define WORLD_H_

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "gb/base/validated_context.h"
#include "gb/render/render_types.h"
#include "glm/glm.hpp"

// Game includes
#include "chunk.h"
#include "game_types.h"
#include "scene_types.h"

// The world class contains the state of the entire game world
class World final {
 public:
  //----------------------------------------------------------------------------
  // Constants and Types
  //----------------------------------------------------------------------------

  static inline constexpr int kMinChunkIndexY = -128 / Chunk::kSize.y;
  static inline constexpr int kMaxChunkIndexY = 256 / Chunk::kSize.y;

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

  //----------------------------------------------------------------------------
  // Chunk access
  //----------------------------------------------------------------------------

  Chunk* GetChunk(const glm::ivec3& index);

  //----------------------------------------------------------------------------
  // Rendering
  //----------------------------------------------------------------------------

  void Draw(const Camera& camera);
  void DrawLightingGui();

 private:
  World(gb::ValidatedContext context);

  bool InitGraphics();

  std::unique_ptr<Chunk> NewChunk(const glm::ivec3& index);
  void InitFlatWorldChunk(Chunk* chunk);
  void InitSineWorldChunk(Chunk* chunk);

  gb::ValidatedContext context_;

  // Game data
  absl::flat_hash_map<std::tuple<int, int, int>, std::unique_ptr<Chunk>>
      chunks_;

  // Render data
  std::unique_ptr<gb::RenderScene> scene_;
  SceneLightData lights_;
};

#endif WORLD_H_
