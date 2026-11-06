// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef RENDER_CHECK_H_
#define RENDER_CHECK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "SDL.h"
#include "absl/base/thread_annotations.h"
#include "absl/log/log_sink.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "gb/game/game.h"
#include "gb/image/pixel.h"
#include "gb/imgui/imgui_instance.h"
#include "gb/message/message_dispatcher.h"
#include "gb/message/message_system.h"
#include "gb/pilot/pilot.h"
#include "gb/pilot/sdl/pilot_sdl.h"
#include "gb/render/draw_list.h"
#include "gb/render/material_config.h"
#include "gb/render/mesh_pool.h"
#include "gb/render/render_system.h"
#include "gb/render/storage_buffer.h"
#include "gb/render/volume_texture.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/resource/resource_set.h"
#include "gb/resource/resource_system.h"

//==============================================================================
// LogCounter
//==============================================================================

// Counts warnings and errors logged from any thread, including the Vulkan
// validation layer's messages, and keeps the most recent ones.
class LogCounter final : public absl::LogSink {
 public:
  struct Counts {
    int warnings = 0;
    int errors = 0;
    std::vector<std::string> recent;
  };

  void Send(const absl::LogEntry& entry) override;

  Counts GetCounts() const;
  void Reset();

 private:
  static constexpr int kMaxRecent = 8;

  mutable absl::Mutex mutex_;
  Counts counts_ ABSL_GUARDED_BY(mutex_);
};

//==============================================================================
// RenderCheck
//==============================================================================

// Exercises the Vulkan backend: window resizing, binding data churn, texture
// and texture array updates while they are drawn, each texture format, volume
// textures, storage buffers, mesh pools and indirect draws, and each material
// blend mode.
//
// The checks are run by hand, or by a pilot in the context, whose scripts (in
// scripts/) press the panel's buttons by their labels. A result that isn't as
// expected logs an error, which fails the pilot's run. The pilot's
// "gui_under_limit" condition holds unless the GUI is being kept over its
// vertex limit.
class RenderCheck final : public gb::Game {
 protected:
  bool Init(const std::vector<std::string_view>& args) override;
  bool Update(absl::Duration delta_time) override;
  void CleanUp() override;

 private:
  // Vertex of the unit quad drawn for each instance.
  struct QuadVertex {
    float x;
    float y;
  };

  // Matches SceneData in shaders/quad.vert and shaders/volume.vert.
  struct SceneData {
    float pixel_to_clip[4];
  };

  // Matches InstanceData in shaders/quad.vert and shaders/volume.vert, which
  // reads the color's x as the depth to sample at.
  struct InstanceData {
    float rect[4];
    float color[4];
  };

  // A volume texture in one format, drawn by a row of quads that sample it at
  // different depths.
  struct VolumeRow {
    const char* name;
    gb::VolumeTexture* volume;
    gb::Material* material;
    std::vector<std::unique_ptr<gb::BindingData>> quads;
  };

  // Matches QuadColor in shaders/storage.frag.
  struct QuadColor {
    float color[4];
  };

  // A storage buffer of colors with one volatility, drawn by a row of quads
  // that each read the color at its own index.
  struct StorageRow {
    const char* name;
    float left;
    gb::StorageBuffer* storage_buffer;
    gb::Material* material;
    std::vector<std::unique_ptr<gb::BindingData>> quads;
  };

  // A texture shown in the GUI, with the result of the last action on it.
  struct ShownTexture {
    gb::Texture* texture = nullptr;
    ImTextureID id = nullptr;
    std::string result;
  };

  bool InitWindow();
  bool InitRenderSystem();
  bool InitGui();
  bool InitQuads();
  bool InitTextures();
  bool InitBlendQuads();
  bool InitFormatTextures();
  bool InitVolumes();
  bool InitStorageBuffers();
  bool InitUnsetBindings();
  bool InitMeshPool();
  bool InitPilot();

  bool ProcessEvents();

  // Keys are shortcuts for some of the panel's checks.
  void OnKeyDown(SDL_Keycode key);

  // Returns false while "Go over the vertex limit for a second" keeps the GUI
  // over its vertex limit.
  bool IsGuiUnderLimit() const;

  void UpdateFrameStats();
  void UpdateInstances();
  void UpdateTextures();
  void UpdateStorageBuffers();
  void UpdateMeshPool();
  void Draw();
  void DrawGui();
  void DrawFrameGui();
  void DrawInstanceGui();
  void DrawTextureGui();
  void DrawTextureArrayGui();
  void DrawGuiCheckGui();
  void DrawBlendGui();
  void DrawFormatGui();
  void DrawVolumeGui();
  void DrawStorageGui();
  void DrawUnsetBindingsGui();
  void DrawMeshPoolGui();
  void DrawLogGui();
  void DrawExtraRects();
  void DrawFormatTextures();
  void AddFormatStripes();
  void DrawVolumeLabels();
  void DrawStorageLabels();
  void DrawUnsetBindingsLabel();
  void DrawMeshPoolLabel();
  void AddVolumeRegions();
  void DrawQuads();
  void DrawVolumes();
  void DrawStorageBuffers();
  void DrawUnsetBindings();
  void DrawMeshPool();

  // Creates the mesh pool's shape at `index`, turned by `angle` radians, or
  // returns null if it couldn't.
  std::unique_ptr<gb::PooledMesh> CreatePoolShape(int index, float angle);

  // Returns the ranges the mesh pool's shapes are drawn with, and writes them
  // to the storage buffer the indirect row is drawn from.
  std::vector<gb::MeshDraw> GetPoolDraws() const;
  void SetPoolDraws();

  void CreateInstances();
  void SetInstanceData(int index, gb::BindingData* instance);
  void SetQuadData(gb::BindingData* instance, float x, float y, float width,
                   float height, gb::Pixel color);
  static QuadColor ToQuadColor(gb::Pixel color);

  // Returns a new material of the quad material type, with its own config.
  gb::Material* CreateQuadMaterial(const gb::MaterialConfig& config);

  // Adds a quad to the blend check's draw list. `material` may be null, if
  // creating it failed, which fails here.
  bool AddBlendQuad(gb::Material* material, float x, float y, float width,
                    float height, gb::Pixel color);
  void PaintSquare(gb::Texture* texture, int step);
  void ResetCheckerboard(gb::Texture* texture);
  void SetArrayLayer(gb::TextureArray* texture_array, int index,
                     gb::Pixel color);
  void TwoRegionUpdate();
  void UpdatePartOfDrawnTexture();
  void UpdatePartOfMipmappedTexture();

  SDL_Window* window_ = nullptr;
  LogCounter log_counter_;
  gb::ThreadMessageDispatcher dispatcher_;
  std::unique_ptr<gb::MessageSystem> message_system_;
  gb::MessageEndpointId sdl_channel_ = gb::kNoMessageEndpointId;
  std::unique_ptr<gb::PilotSdl> pilot_sdl_;
  gb::PilotHandle gui_under_limit_;
  std::unique_ptr<gb::ResourceSystem> resource_system_;
  std::unique_ptr<gb::RenderSystem> render_system_;
  gb::VulkanBackend* backend_ = nullptr;
  std::unique_ptr<gb::ImGuiInstance> gui_instance_;
  gb::ResourceSet resources_;

  // Frame statistics, and the game time run.
  int frame_ = 0;
  absl::Duration time_;
  int frames_drawn_ = 0;
  int begin_frame_failures_ = 0;
  int64_t gpu_bytes_ = 0;
  int64_t peak_gpu_bytes_ = 0;
  int64_t process_bytes_ = 0;
  int64_t peak_process_bytes_ = 0;

  // Instance churn.
  std::unique_ptr<gb::RenderScene> scene_;
  gb::Material* quad_material_ = nullptr;
  gb::Mesh* quad_mesh_ = nullptr;
  std::vector<std::unique_ptr<gb::BindingData>> instances_;
  bool churn_ = false;
  int64_t instances_created_ = 0;
  int failed_instances_ = 0;

  // Textures.
  ShownTexture read_write_texture_;
  ShownTexture per_frame_texture_;
  ShownTexture static_write_texture_;
  bool paint_ = false;
  int paint_step_ = 0;

  // Texture arrays.
  gb::TextureArray* read_write_array_ = nullptr;
  gb::TextureArray* static_write_array_ = nullptr;
  std::vector<ImTextureID> read_write_array_ids_;
  std::vector<ImTextureID> static_write_array_ids_;
  bool cycle_layer_ = false;
  std::string read_write_array_result_;
  std::string static_write_array_result_;

  // A texture in each format, drawn by DrawFormatTextures().
  struct FormatTexture {
    const char* name;
    gb::Texture* texture;
    ImTextureID id;
  };
  std::vector<FormatTexture> format_textures_;

  // Volume textures, drawn by DrawVolumes().
  std::vector<VolumeRow> volume_rows_;

  // Storage buffers, drawn by DrawStorageBuffers(), with the second of game
  // time at which their last color was last changed.
  std::vector<StorageRow> storage_rows_;
  int64_t storage_second_ = -1;

  // Reads its color from a storage buffer of QuadColor, by an index from the
  // vertex shader. The mesh pool's shapes use it too.
  gb::Shader* storage_fragment_shader_ = nullptr;

  // A quad drawn by DrawUnsetBindings(), whose material's bindings are never
  // set.
  gb::Material* unset_material_ = nullptr;
  std::unique_ptr<gb::BindingData> unset_quad_;

  // A mesh pool, drawn by DrawMeshPool(), whose shapes each read their color
  // from a storage buffer by their draw index. While replacing, one shape is
  // replaced each frame. The shapes are drawn twice: a range at a time with
  // `pool_instance_`, and in one indirect draw from `pool_draws_` with
  // `pool_indirect_instance_`, which moves them down. `multi_draw_` sets
  // whether the indirect draw may use multi-draw.
  gb::MeshPool* mesh_pool_ = nullptr;
  gb::Material* pool_material_ = nullptr;
  std::unique_ptr<gb::BindingData> pool_instance_;
  std::unique_ptr<gb::BindingData> pool_indirect_instance_;
  gb::StorageBuffer* pool_draws_ = nullptr;
  std::vector<std::unique_ptr<gb::PooledMesh>> pool_shapes_;
  bool replace_shapes_ = false;
  bool multi_draw_ = true;
  int64_t shapes_replaced_ = 0;

  // GUI.
  int extra_rects_ = 0;
  int over_limit_frames_ = 0;
  bool reset_render_state_ = false;

  // Blend modes, drawn in order from a list built once.
  std::vector<std::unique_ptr<gb::BindingData>> blend_instances_;
  gb::DrawList blend_draw_list_;
};

#endif  // RENDER_CHECK_H_
