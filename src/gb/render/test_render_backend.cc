// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_render_backend.h"

namespace gb {

void TestRenderBackend::State::ResetCounts() { invalid_call_count = 0; }

void TestRenderBackend::State::ResetState() {
  State state;
  state.backend = backend;
  *this = state;
}

FrameDimensions TestRenderBackend::GetFrameDimensions(RenderInternal) const {
  return state_->frame_dimensions;
}

Texture* TestRenderBackend::CreateTexture(RenderInternal, ResourceEntry entry,
                                          DataVolatility volatility, int width,
                                          int height) {
  if (state_->fail_create_texture) {
    return nullptr;
  }
  return new TestTexture(&state_->texture_config, std::move(entry), volatility,
                         width, height);
}

std::unique_ptr<ShaderCode> TestRenderBackend::CreateShaderCode(RenderInternal,
                                                const void* code,
                                                int64_t code_size) {
  if (state_->fail_create_shader_code) {
    return nullptr;
  }
  return std::make_unique<TestShaderCode>(code, code_size);
}

std::unique_ptr<RenderSceneType> TestRenderBackend::CreateSceneType(
    RenderInternal, absl::Span<const Binding> bindings) {
  if (state_->fail_create_scene_type) {
    return nullptr;
  }
  return std::make_unique<TestRenderSceneType>(bindings);
}

std::unique_ptr<RenderScene> TestRenderBackend::CreateScene(
    RenderInternal, RenderSceneType* scene_type, int scene_order) {
  if (state_->fail_create_scene) {
    return nullptr;
  }
  return std::make_unique<TestRenderScene>(scene_type, scene_order);
}

std::unique_ptr<RenderPipeline> TestRenderBackend::CreatePipeline(
    RenderInternal, RenderSceneType* scene_type, const VertexType* vertex_type,
    absl::Span<const Binding> bindings, ShaderCode* vertex_shader,
    ShaderCode* fragment_shader) {
  if (state_->fail_create_pipeline) {
    return nullptr;
  }
  return std::make_unique<TestRenderPipeline>(
      &state_->render_pipeline_config,
      static_cast<TestRenderSceneType*>(scene_type), vertex_type, bindings,
      static_cast<TestShaderCode*>(vertex_shader),
      static_cast<TestShaderCode*>(fragment_shader));
}

std::unique_ptr<RenderBuffer> TestRenderBackend::CreateVertexBuffer(
    RenderInternal, DataVolatility volatility, int vertex_size,
    int vertex_capacity) {
  if (state_->fail_create_vertex_buffer) {
    return nullptr;
  }
  return std::make_unique<TestRenderBuffer>(
      &state_->vertex_buffer_config, volatility, vertex_size, vertex_capacity);
}

std::unique_ptr<RenderBuffer> TestRenderBackend::CreateIndexBuffer(
    RenderInternal, DataVolatility volatility, int index_capacity) {
  if (state_->fail_create_index_buffer) {
    return nullptr;
  }
  return std::make_unique<TestRenderBuffer>(
      &state_->index_buffer_config, volatility,
      static_cast<int>(sizeof(uint16_t)), index_capacity);
}

bool TestRenderBackend::BeginFrame(RenderInternal) {
  if (state_->fail_begin_frame) {
    return false;
  }
  if (state_->rendering) {
    state_->invalid_call_count += 1;
  }
  state_->rendering = true;
  return true;
}

void TestRenderBackend::Draw(RenderInternal, RenderScene* scene,
                             RenderPipeline* pipeline,
                             BindingData* material_data,
                             BindingData* instance_data, RenderBuffer* vertices,
                             RenderBuffer* indices) {
  if (!state_->rendering || scene == nullptr || pipeline == nullptr ||
      material_data == nullptr || instance_data == nullptr ||
      vertices == nullptr || indices == nullptr) {
    state_->invalid_call_count += 1;
  }
  state_->draw_list.push_back({static_cast<TestRenderScene*>(scene),
                               static_cast<TestRenderPipeline*>(pipeline),
                               static_cast<TestBindingData*>(material_data),
                               static_cast<TestBindingData*>(instance_data),
                               static_cast<TestRenderBuffer*>(vertices),
                               static_cast<TestRenderBuffer*>(indices)});
}

void TestRenderBackend::EndFrame(RenderInternal) {
  if (!state_->rendering) {
    state_->invalid_call_count += 1;
  }
  state_->rendering = false;
}

}  // namespace gb
