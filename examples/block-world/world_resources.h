#ifndef WORLD_RESOURCES_H_
#define WORLD_RESOURCES_H_

#include <memory>

#include "gb/base/validated_context.h"
#include "gb/render/material.h"
#include "gb/render/render_scene_type.h"
#include "gb/resource/resource_set.h"

// This class contains all world resources required for every world.
class WorldResources {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: ResourceSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintResourceSystem, kInRequired,
                               gb::ResourceSystem);

  // REQUIRED: RenderSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintRenderSystem, kInRequired,
                               gb::RenderSystem);

  using Contract =
      gb::ContextContract<kConstraintResourceSystem, kConstraintRenderSystem>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  WorldResources(const WorldResources&) = delete;
  WorldResources(WorldResources&&) = delete;
  WorldResources& operator=(const WorldResources&) = delete;
  WorldResources& operator=(WorldResources&&) = delete;
  ~WorldResources();

  static std::unique_ptr<WorldResources> Create(Contract contract);

  //----------------------------------------------------------------------------
  // Render resources
  //----------------------------------------------------------------------------

  gb::RenderSceneType* GetSceneType() const { return scene_type_; }
  gb::Material* GetChunkMaterial() const { return chunk_material_; }

 private:
  WorldResources(gb::ValidatedContext context);

  bool InitGraphics();

  gb::ValidatedContext context_;
  gb::ResourceSet resources_;
  gb::RenderSceneType* scene_type_;
  gb::Material* chunk_material_;
};

#endif  // WORLD_RESOURCES_H_
