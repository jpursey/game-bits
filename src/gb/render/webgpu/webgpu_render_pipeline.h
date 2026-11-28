// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_RENDER_PIPELINE_H_
#define GB_RENDER_WEBGPU_WEBGPU_RENDER_PIPELINE_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gb/render/binding.h"
#include "gb/render/material_config.h"
#include "gb/render/render_pipeline.h"
#include "gb/render/render_types.h"
#include "gb/render/webgpu/webgpu_backend.h"
#include "gb/render/webgpu/webgpu_binding_data.h"
#include "gb/render/webgpu/webgpu_binding_layout.h"
#include "gb/render/webgpu/webgpu_shader_code.h"
#include "webgpu/webgpu_cpp.h"

namespace gb {

// The kind of pass a pipeline draws in, which WebGPU bakes into the pipeline.
struct WebGpuPassKind {
  template <typename H>
  friend H AbslHashValue(H h, const WebGpuPassKind& kind) {
    return H::combine(std::move(h), kind.color_format, kind.depth_format,
                      kind.depth_read_only, kind.sample_count);
  }
  bool operator==(const WebGpuPassKind&) const = default;

  // Undefined for a pass with no color or depth target.
  wgpu::TextureFormat color_format = wgpu::TextureFormat::Undefined;
  wgpu::TextureFormat depth_format = wgpu::TextureFormat::Undefined;
  bool depth_read_only = false;
  int sample_count = 1;
};

// WebGPU implementation of a render pipeline.
//
// It holds a wgpu::RenderPipeline for each kind of pass it draws in, made the
// first time it is drawn in it, and the bind group layouts of its material
// and instance binding data.
//
// This class is thread-safe.
class WebGpuRenderPipeline final : public RenderPipeline {
 public:
  // Creates the pipeline, and its wgpu::RenderPipeline for `frame_kind`.
  // Returns null, having logged why, if WebGPU can't draw with it, such as for
  // RasterMode::kLine or a 3-channel 8 or 16-bit vertex attribute. The shaders
  // and `backend` must outlive it.
  static std::unique_ptr<WebGpuRenderPipeline> Create(
      WebGpuBackend* backend, WebGpuBindingLayout* binding_layout,
      const VertexType* vertex_type, absl::Span<const Binding> bindings,
      WebGpuShaderCode* vertex_shader, WebGpuShaderCode* fragment_shader,
      const MaterialConfig& config, const WebGpuPassKind& frame_kind);

  // Returns the wgpu::RenderPipeline that draws in passes of `kind`, made the
  // first time it is asked for. Returns null if it can't be made, which is
  // logged once.
  wgpu::RenderPipeline Get(const WebGpuPassKind& kind);

  std::unique_ptr<BindingData> CreateMaterialBindingData() override;
  std::unique_ptr<BindingData> CreateInstanceBindingData() override;
  bool ValidateInstanceBindingData(BindingData* binding_data) override;

 private:
  WebGpuRenderPipeline(WebGpuBackend* backend, WebGpuShaderCode* vertex_shader,
                       WebGpuShaderCode* fragment_shader,
                       const MaterialConfig& config)
      : backend_(backend),
        vertex_shader_(vertex_shader),
        fragment_shader_(fragment_shader),
        config_(config) {}

  bool Init(WebGpuBindingLayout* binding_layout, const VertexType* vertex_type,
            absl::Span<const Binding> bindings);

  // Makes the wgpu::RenderPipeline for passes of `kind`, or returns null,
  // having logged why.
  wgpu::RenderPipeline CreatePipeline(const WebGpuPassKind& kind);

  WebGpuBackend* const backend_;
  WebGpuShaderCode* const vertex_shader_;
  WebGpuShaderCode* const fragment_shader_;
  const MaterialConfig config_;
  int vertex_size_ = 0;
  std::vector<wgpu::VertexAttribute> vertex_attributes_;
  std::shared_ptr<const WebGpuBindingSetLayout> material_layout_;
  std::shared_ptr<const WebGpuBindingSetLayout> instance_layout_;
  wgpu::PipelineLayout pipeline_layout_;

  absl::Mutex mutex_;

  // Null for a kind of pass whose pipeline failed to be made.
  absl::flat_hash_map<WebGpuPassKind, wgpu::RenderPipeline> pipelines_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_RENDER_PIPELINE_H_
