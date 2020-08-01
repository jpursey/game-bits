// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_RENDER_BACKEND_H_
#define GB_RENDER_TEST_RENDER_BACKEND_H_

#include "gb/render/render_backend.h"
#include "gb/render/test_binding_data.h"
#include "gb/render/test_render_buffer.h"
#include "gb/render/test_render_pipeline.h"
#include "gb/render/test_render_scene.h"
#include "gb/render/test_render_scene_type.h"
#include "gb/render/test_shader_code.h"
#include "gb/render/test_texture.h"
#include "glog/logging.h"

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
    bool rendering = false;
    std::vector<DrawItem> draw_list;
    TestTexture::Config texture_config;
    TestRenderPipeline::Config render_pipeline_config;
    TestRenderBuffer::Config vertex_buffer_config;
    TestRenderBuffer::Config index_buffer_config;

    bool fail_create_texture = false;
    bool fail_create_shader_code = false;
    bool fail_create_scene_type = false;
    bool fail_create_scene = false;
    bool fail_create_pipeline = false;
    bool fail_create_vertex_buffer = false;
    bool fail_create_index_buffer = false;
    bool fail_begin_frame = false;

    int invalid_call_count = 0;
  };

  TestRenderBackend(State* state) : state_(state) {
    CHECK(state_->backend == nullptr);
    state_->backend = this;
  }
  ~TestRenderBackend() override {
    CHECK(state_->backend == this);
    state_->backend = nullptr;
  }

  // Implementation of RenderBackend
  FrameDimensions GetFrameDimensions(RenderInternal) const override;
  Texture* CreateTexture(RenderInternal, ResourceEntry entry,
                         DataVolatility volatility, int width,
                         int height) override;
  ShaderCode* CreateShaderCode(RenderInternal, ResourceEntry entry,
                               const void* code, int64_t code_size) override;
  std::unique_ptr<RenderSceneType> CreateSceneType(
      RenderInternal, absl::Span<const Binding> bindings) override;
  std::unique_ptr<RenderScene> CreateScene(RenderInternal,
                                           RenderSceneType* scene_type,
                                           int scene_order) override;
  std::unique_ptr<RenderPipeline> CreatePipeline(
      RenderInternal, RenderSceneType* scene_type,
      const VertexType* vertex_type, absl::Span<const Binding> bindings,
      ShaderCode* vertex_shader, ShaderCode* fragment_shader) override;
  std::unique_ptr<RenderBuffer> CreateVertexBuffer(
      RenderInternal, DataVolatility volatility, int vertex_size,
      int vertex_capacity) override;
  std::unique_ptr<RenderBuffer> CreateIndexBuffer(RenderInternal,
                                                  DataVolatility volatility,
                                                  int index_capacity) override;
  bool BeginFrame(RenderInternal) override;
  void Draw(RenderInternal, RenderScene* scene, RenderPipeline* pipeline,
            BindingData* material_data, BindingData* instance_data,
            RenderBuffer* vertices, RenderBuffer* indices) override;
  void EndFrame(RenderInternal) override;

 private:
  State* state_ = nullptr;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_RENDER_BACKEND_H_
