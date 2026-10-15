// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef RENDER_CHECK_H_
#define RENDER_CHECK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SDL.h"
#include "absl/base/thread_annotations.h"
#include "absl/log/log_sink.h"
#include "absl/synchronization/mutex.h"
#include "gb/file/file_system.h"
#include "gb/imgui/imgui_instance.h"
#include "gb/message/message_dispatcher.h"
#include "gb/message/message_system.h"
#include "gb/render/render_system.h"
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

// Exercises the Vulkan backend by hand: window resizing, binding data churn,
// and texture and texture array updates while they are drawn.
class RenderCheck final {
 public:
  RenderCheck() = default;
  RenderCheck(const RenderCheck&) = delete;
  RenderCheck& operator=(const RenderCheck&) = delete;
  ~RenderCheck();

  bool Init();
  void Run();

 private:
  // Vertex of the unit quad drawn for each instance.
  struct QuadVertex {
    float x;
    float y;
  };

  // Matches SceneData in shaders/quad.vert.
  struct SceneData {
    float pixel_to_clip[4];
  };

  // Matches InstanceData in shaders/quad.vert.
  struct InstanceData {
    float rect[4];
    float color[4];
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
  void CleanUp();

  bool ProcessEvents();
  void UpdateFrameStats();
  void UpdateInstances();
  void UpdateTextures();
  void DrawGui();
  void DrawFrameGui();
  void DrawInstanceGui();
  void DrawTextureGui();
  void DrawTextureArrayGui();
  void DrawGuiCheckGui();
  void DrawLogGui();
  void DrawExtraRects();
  void DrawQuads();

  void CreateInstances();
  void SetInstanceData(int index, gb::BindingData* instance);
  void PaintSquare(gb::Texture* texture, int step);
  void ResetCheckerboard(gb::Texture* texture);
  void SetArrayLayer(gb::TextureArray* texture_array, int index,
                     gb::Pixel color);
  void TwoRegionUpdate();

  SDL_Window* window_ = nullptr;
  LogCounter log_counter_;
  gb::ThreadMessageDispatcher dispatcher_;
  std::unique_ptr<gb::MessageSystem> message_system_;
  gb::MessageEndpointId sdl_channel_ = gb::kNoMessageEndpointId;
  std::unique_ptr<gb::FileSystem> file_system_;
  std::unique_ptr<gb::ResourceSystem> resource_system_;
  std::unique_ptr<gb::RenderSystem> render_system_;
  gb::VulkanBackend* backend_ = nullptr;
  std::unique_ptr<gb::ImGuiInstance> gui_instance_;
  gb::ResourceSet resources_;

  // Frame statistics.
  int frame_ = 0;
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

  // GUI.
  int extra_rects_ = 0;
  int over_limit_frames_ = 0;
  bool reset_render_state_ = false;
};

#endif  // RENDER_CHECK_H_
