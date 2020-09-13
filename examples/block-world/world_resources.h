#ifndef WORLD_RESOURCES_H_
#define WORLD_RESOURCES_H_

#include <memory>

#include "gb/base/validated_context.h"
#include "gb/imgui/imgui_types.h"
#include "gb/render/material.h"
#include "gb/render/render_scene_type.h"
#include "gb/resource/resource_set.h"
#include "imgui.h"

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

  // REQUIRED: ImGuiInstance interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintGuiInstance, kInRequired,
                               gb::ImGuiInstance);

  using Contract =
      gb::ContextContract<kConstraintResourceSystem, kConstraintRenderSystem,
                          kConstraintGuiInstance>;

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

  //----------------------------------------------------------------------------
  // GUI resources
  //----------------------------------------------------------------------------

  ImTextureID GetBlockGuiTexture() const { return chunk_gui_texture_; }

 private:
  WorldResources(gb::ValidatedContext context);

  bool InitGraphics();
  bool InitGui();

  gb::ValidatedContext context_;
  gb::ResourceSet resources_;
  gb::RenderSceneType* scene_type_ = nullptr;
  gb::Material* chunk_material_ = nullptr;
  gb::Texture* block_texture_ = nullptr;
  ImTextureID chunk_gui_texture_ = nullptr;
};

#endif  // WORLD_RESOURCES_H_
