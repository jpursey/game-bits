// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_RENDER_PIPELINE_H_
#define GB_RENDER_VULKAN_VULKAN_RENDER_PIPELINE_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gb/render/material_config.h"
#include "gb/render/render_pipeline.h"
#include "gb/render/vulkan/vulkan_binding_data_factory.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Vulkan implementation of RenderPipeline.
//
// It holds a vk::Pipeline for each layout of render pass it draws in, as a
// vk::Pipeline only draws in render passes of the layout it was created for.
//
// This class is thread-safe.
class VulkanRenderPipeline final : public RenderPipeline {
 public:
  // Creates the pipeline, and its vk::Pipeline for the frame's render pass.
  // The shaders must outlive it.
  static std::unique_ptr<VulkanRenderPipeline> Create(
      VulkanInternal, VulkanBackend* backend,
      VulkanBindingLayout* binding_layout, const VertexType* vertex_type,
      absl::Span<const Binding> bindings, VulkanShaderCode* vertex_shader,
      VulkanShaderCode* fragment_shader, const MaterialConfig& config,
      const VulkanRenderPassInfo& frame_pass);
  ~VulkanRenderPipeline() override;

  // Returns the vk::Pipeline that draws in render passes of `pass`'s layout,
  // created from `pass` the first time it is asked for. Returns null if it
  // can't be created, which is logged once.
  vk::Pipeline Get(const VulkanRenderPassInfo& pass);

  vk::PipelineLayout GetLayout() const { return pipeline_layout_; }

  std::unique_ptr<BindingData> CreateMaterialBindingData() override;
  std::unique_ptr<BindingData> CreateInstanceBindingData() override;
  bool ValidateInstanceBindingData(BindingData* binding_data) override;

 private:
  VulkanRenderPipeline(VulkanBackend* backend, VulkanShaderCode* vertex_shader,
                       VulkanShaderCode* fragment_shader,
                       const MaterialConfig& config);

  bool Init(VulkanBindingLayout* binding_layout, const VertexType* vertex_type,
            absl::Span<const Binding> bindings);

  // Creates the vk::Pipeline for render passes of `pass`'s layout. Returns
  // null if it fails.
  vk::Pipeline CreatePipeline(const VulkanRenderPassInfo& pass);

  VulkanBackend* const backend_;
  VulkanShaderCode* const vertex_shader_;
  VulkanShaderCode* const fragment_shader_;
  const MaterialConfig config_;
  vk::VertexInputBindingDescription vertex_binding_;
  std::vector<vk::VertexInputAttributeDescription> vertex_attributes_;
  std::unique_ptr<VulkanBindingDataFactory> material_data_factory_;
  std::unique_ptr<VulkanBindingDataFactory> instance_data_factory_;
  vk::PipelineLayout pipeline_layout_;

  absl::Mutex mutex_;

  // Null for a layout whose pipeline failed to be created.
  absl::flat_hash_map<VulkanPassLayout, vk::Pipeline> pipelines_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_RENDER_PIPELINE_H_
