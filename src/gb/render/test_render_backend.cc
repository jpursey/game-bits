// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_render_backend.h"

#include <memory>

#include "absl/log/log.h"
#include "gb/image/image.h"

namespace gb {

void TestRenderBackend::State::ResetCounts() {
  invalid_call_count = 0;
  read_image_count = 0;
}

void TestRenderBackend::State::ResetState() {
  State state;
  state.backend = backend;
  *this = state;
}

void TestRenderBackend::AddMeshPoolBuffers(TestMeshPoolBuffers* buffers) {
  absl::MutexLock lock(&mutex_);
  mesh_pool_buffers_.push_back(buffers);
}

void TestRenderBackend::RemoveMeshPoolBuffers(TestMeshPoolBuffers* buffers) {
  absl::MutexLock lock(&mutex_);
  std::erase(mesh_pool_buffers_, buffers);
}

FrameDimensions TestRenderBackend::GetFrameDimensions(RenderInternal) const {
  return state_->frame_dimensions;
}

bool TestRenderBackend::CanFilter(RenderInternal, TextureFormat format) const {
  return !state_->unfilterable_formats.contains(format);
}

Texture* TestRenderBackend::CreateTexture(RenderInternal, ResourceEntry entry,
                                          DataVolatility volatility, int width,
                                          int height,
                                          const SamplerOptions& options,
                                          TextureFormat format) {
  if (state_->fail_create_texture) {
    return nullptr;
  }
  return new TestTexture(&state_->texture_config, std::move(entry), volatility,
                         width, height, options, format);
}

TextureArray* TestRenderBackend::CreateTextureArray(
    RenderInternal, ResourceEntry entry, DataVolatility volatility, int count,
    int width, int height, const SamplerOptions& options,
    TextureFormat format) {
  if (state_->fail_create_texture_array) {
    return nullptr;
  }
  return new TestTextureArray(&state_->texture_array_config, std::move(entry),
                              volatility, count, width, height, options,
                              format);
}

VolumeTexture* TestRenderBackend::CreateVolumeTexture(
    RenderInternal, ResourceEntry entry, DataVolatility volatility, int width,
    int height, int depth, const SamplerOptions& options,
    TextureFormat format) {
  if (state_->fail_create_volume_texture) {
    return nullptr;
  }
  return new TestVolumeTexture(&state_->volume_texture_config, std::move(entry),
                               volatility, width, height, depth, options,
                               format);
}

RenderTarget* TestRenderBackend::CreateRenderTarget(
    RenderInternal, ResourceEntry entry, TextureFormat format,
    const RenderTargetSize& size, int width, int height,
    const SamplerOptions& options) {
  if (state_->fail_create_render_target) {
    return nullptr;
  }
  return new TestRenderTarget(&state_->render_target_config, std::move(entry),
                              format, size, width, height, options);
}

std::unique_ptr<ShaderCode> TestRenderBackend::CreateShaderCode(
    RenderInternal, const void* code, int64_t code_size) {
  if (state_->fail_create_shader_code) {
    return nullptr;
  }
  return std::make_unique<TestShaderCode>(code, code_size);
}

std::unique_ptr<BindingLayout> TestRenderBackend::CreateBindingLayout(
    RenderInternal, absl::Span<const Binding> bindings) {
  if (state_->fail_create_binding_layout) {
    return nullptr;
  }
  return std::make_unique<TestBindingLayout>(bindings);
}

std::unique_ptr<RenderView> TestRenderBackend::CreateView(
    RenderInternal, BindingLayout* binding_layout, int view_order) {
  if (state_->fail_create_view) {
    return nullptr;
  }
  return std::make_unique<TestRenderView>(binding_layout, view_order);
}

std::unique_ptr<RenderPipeline> TestRenderBackend::CreatePipeline(
    RenderInternal, BindingLayout* binding_layout,
    const VertexType* vertex_type, absl::Span<const Binding> bindings,
    ShaderCode* vertex_shader, ShaderCode* fragment_shader,
    const MaterialConfig& config) {
  if (state_->fail_create_pipeline) {
    return nullptr;
  }
  return std::make_unique<TestRenderPipeline>(
      &state_->render_pipeline_config,
      static_cast<TestBindingLayout*>(binding_layout), vertex_type, bindings,
      static_cast<TestShaderCode*>(vertex_shader),
      static_cast<TestShaderCode*>(fragment_shader), config);
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

std::unique_ptr<RenderBuffer> TestRenderBackend::CreateStorageBuffer(
    RenderInternal, DataVolatility volatility, int value_size, int capacity) {
  if (state_->fail_create_storage_buffer) {
    return nullptr;
  }
  return std::make_unique<TestRenderBuffer>(&state_->storage_buffer_config,
                                            volatility, value_size, capacity);
}

std::unique_ptr<MeshPoolBuffers> TestRenderBackend::CreateMeshPoolBuffers(
    RenderInternal, int vertex_size, IndexType index_type, int vertex_capacity,
    int index_capacity) {
  if (state_->fail_create_mesh_pool_buffers) {
    return nullptr;
  }
  return std::make_unique<TestMeshPoolBuffers>(
      &state_->mesh_pool_buffers_config, this, vertex_size, index_type,
      vertex_capacity, index_capacity);
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

void TestRenderBackend::Draw(RenderInternal, RenderView* view,
                             RenderPipeline* pipeline,
                             BindingData* material_data,
                             BindingData* instance_data, RenderBuffer* vertices,
                             RenderBuffer* indices) {
  if (!state_->rendering || view == nullptr || pipeline == nullptr ||
      material_data == nullptr || instance_data == nullptr ||
      vertices == nullptr || indices == nullptr) {
    state_->invalid_call_count += 1;
  }
  state_->draw_list.push_back({static_cast<TestRenderView*>(view),
                               static_cast<TestRenderPipeline*>(pipeline),
                               static_cast<TestBindingData*>(material_data),
                               static_cast<TestBindingData*>(instance_data),
                               static_cast<TestRenderBuffer*>(vertices),
                               static_cast<TestRenderBuffer*>(indices)});
}

void TestRenderBackend::Draw(RenderInternal, RenderView* view,
                             absl::Span<const DrawCommand> commands) {
  if (!state_->rendering || view == nullptr) {
    state_->invalid_call_count += 1;
  }
  state_->draw_commands.insert(state_->draw_commands.end(), commands.begin(),
                               commands.end());
}

void TestRenderBackend::EndFrame(RenderInternal) {
  if (!state_->rendering) {
    state_->invalid_call_count += 1;
  }
  state_->rendering = false;
  {
    absl::MutexLock lock(&mutex_);
    for (TestMeshPoolBuffers* buffers : mesh_pool_buffers_) {
      buffers->CopyWrites();
    }
  }
  frame_.fetch_add(1, std::memory_order_release);
}

std::unique_ptr<Image> TestRenderBackend::EndFrameAndReadImage(
    RenderInternal access_token) {
  EndFrame(access_token);
  state_->read_image_count += 1;
  if (state_->fail_read_image) {
    LOG(ERROR) << "The test backend's image read was set to fail";
    return nullptr;
  }
  return std::make_unique<Image>(state_->frame_dimensions.width,
                                 state_->frame_dimensions.height,
                                 state_->clear_color);
}

}  // namespace gb
