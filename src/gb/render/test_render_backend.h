// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_RENDER_BACKEND_H_
#define GB_RENDER_TEST_RENDER_BACKEND_H_

#include <atomic>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "gb/render/draw_list.h"
#include "gb/render/render_backend.h"
#include "gb/render/render_types.h"
#include "gb/render/test_binding_data.h"
#include "gb/render/test_mesh_pool_buffers.h"
#include "gb/render/test_render_buffer.h"
#include "gb/render/test_render_pipeline.h"
#include "gb/render/test_render_scene.h"
#include "gb/render/test_texture.h"
#include "gb/render/test_texture_array.h"
#include "gb/render/test_volume_texture.h"

namespace gb {

// Implementation of RenderBackend for use in tests.
class TestRenderBackend final : public RenderBackend {
 public:
  struct DrawItem {
    TestRenderScene* scene;
    TestRenderPipeline* pipeline;
    TestBindingData* material_data;
    TestBindingData* instance_data;
    TestRenderBuffer* vertices;
    TestRenderBuffer* indices;
  };

  struct State {
    State() = default;

    void ResetCounts();
    void ResetState();

    TestRenderBackend* backend = nullptr;  // Non-null when part of a backend.

    FrameDimensions frame_dimensions = {0, 0};

    // The color of every pixel EndFrameAndReadImage() returns, as nothing is
    // drawn.
    Pixel clear_color = {0, 0, 0, 255};

    bool rendering = false;
    std::vector<DrawItem> draw_list;
    std::vector<DrawCommand> draw_commands;  // From every DrawList drawn.
    TestTexture::Config texture_config;
    TestTextureArray::Config texture_array_config;
    TestVolumeTexture::Config volume_texture_config;
    TestRenderPipeline::Config render_pipeline_config;
    TestRenderBuffer::Config vertex_buffer_config;
    TestRenderBuffer::Config index_buffer_config;
    TestRenderBuffer::Config storage_buffer_config;
    TestMeshPoolBuffers::Config mesh_pool_buffers_config;

    // Formats CanFilter() says can't be filtered.
    absl::flat_hash_set<TextureFormat> unfilterable_formats;

    bool fail_create_texture = false;
    bool fail_create_texture_array = false;
    bool fail_create_volume_texture = false;
    bool fail_create_shader_code = false;
    bool fail_create_scene_type = false;
    bool fail_create_scene = false;
    bool fail_create_pipeline = false;
    bool fail_create_vertex_buffer = false;
    bool fail_create_index_buffer = false;
    bool fail_create_storage_buffer = false;
    bool fail_create_mesh_pool_buffers = false;
    bool fail_begin_frame = false;
    bool fail_read_image = false;

    int invalid_call_count = 0;
    int read_image_count = 0;  // Calls to EndFrameAndReadImage().
  };

  TestRenderBackend(State* state) : state_(state) {
    CHECK(state_->backend == nullptr);
    state_->backend = this;
  }
  ~TestRenderBackend() override {
    CHECK(state_->backend == this);
    state_->backend = nullptr;
  }

  // Returns the number of the frame being recorded, which starts at zero and
  // goes up by one each time EndFrame() is called. Thread-safe.
  int GetFrame() const { return frame_.load(std::memory_order_acquire); }

  // Called by TestMeshPoolBuffers as it is created and destroyed, so the
  // buffers' writes are copied when each frame ends.
  void AddMeshPoolBuffers(TestMeshPoolBuffers* buffers);
  void RemoveMeshPoolBuffers(TestMeshPoolBuffers* buffers);

  // Implementation of RenderBackend
  void SetClearColor(RenderInternal, Pixel color) override {
    state_->clear_color = color;
  }
  FrameDimensions GetFrameDimensions(RenderInternal) const override;
  bool CanFilter(RenderInternal, TextureFormat format) const override;
  Texture* CreateTexture(RenderInternal, ResourceEntry entry,
                         DataVolatility volatility, int width, int height,
                         const SamplerOptions& options,
                         TextureFormat format) override;
  TextureArray* CreateTextureArray(RenderInternal, ResourceEntry entry,
                                   DataVolatility volatility, int count,
                                   int width, int height,
                                   const SamplerOptions& options,
                                   TextureFormat format) override;
  VolumeTexture* CreateVolumeTexture(RenderInternal, ResourceEntry entry,
                                     DataVolatility volatility, int width,
                                     int height, int depth,
                                     const SamplerOptions& options,
                                     TextureFormat format) override;
  std::unique_ptr<ShaderCode> CreateShaderCode(RenderInternal, const void* code,
                                               int64_t code_size) override;
  std::unique_ptr<RenderSceneType> CreateSceneType(
      RenderInternal, absl::Span<const Binding> bindings) override;
  std::unique_ptr<RenderScene> CreateScene(RenderInternal,
                                           RenderSceneType* scene_type,
                                           int scene_order) override;
  std::unique_ptr<RenderPipeline> CreatePipeline(
      RenderInternal, RenderSceneType* scene_type,
      const VertexType* vertex_type, absl::Span<const Binding> bindings,
      ShaderCode* vertex_shader, ShaderCode* fragment_shader,
      const MaterialConfig& config) override;
  std::unique_ptr<RenderBuffer> CreateVertexBuffer(
      RenderInternal, DataVolatility volatility, int vertex_size,
      int vertex_capacity) override;
  std::unique_ptr<RenderBuffer> CreateIndexBuffer(RenderInternal,
                                                  DataVolatility volatility,
                                                  int index_capacity) override;
  std::unique_ptr<RenderBuffer> CreateStorageBuffer(RenderInternal,
                                                    DataVolatility volatility,
                                                    int value_size,
                                                    int capacity) override;
  std::unique_ptr<MeshPoolBuffers> CreateMeshPoolBuffers(
      RenderInternal, int vertex_size, IndexType index_type,
      int vertex_capacity, int index_capacity) override;
  bool BeginFrame(RenderInternal) override;
  void Draw(RenderInternal, RenderScene* scene, RenderPipeline* pipeline,
            BindingData* material_data, BindingData* instance_data,
            RenderBuffer* vertices, RenderBuffer* indices) override;
  void Draw(RenderInternal, RenderScene* scene,
            absl::Span<const DrawCommand> commands) override;
  void EndFrame(RenderInternal) override;
  std::unique_ptr<Image> EndFrameAndReadImage(RenderInternal) override;

 private:
  State* state_ = nullptr;
  std::atomic<int> frame_ = 0;

  absl::Mutex mutex_;
  std::vector<TestMeshPoolBuffers*> mesh_pool_buffers_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace gb

#endif  // GB_RENDER_TEST_RENDER_BACKEND_H_
