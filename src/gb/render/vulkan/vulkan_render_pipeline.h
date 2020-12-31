// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_RENDER_PIPELINE_H_
#define GB_RENDER_VULKAN_VULKAN_RENDER_PIPELINE_H_

#include <vector>

#include "gb/render/render_pipeline.h"
#include "gb/render/vulkan/vulkan_descriptor_pool.h"
#include "gb/render/vulkan/vulkan_render_buffer.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Vulkan implementation of RenderPipeline.
//
// This class is thread-safe.
class VulkanRenderPipeline final : public RenderPipeline {
 public:
  static std::unique_ptr<VulkanRenderPipeline> Create(
      VulkanInternal, VulkanBackend* backend, VulkanSceneType* scene_type,
      const VertexType* vertex_type, absl::Span<const Binding> bindings,
      VulkanShaderCode* vertex_shader, VulkanShaderCode* fragment_shader,
      const MaterialConfig& config, vk::RenderPass render_pass);
  ~VulkanRenderPipeline() override;

  vk::Pipeline Get() const { return pipeline_; }
  vk::PipelineLayout GetLayout() const { return pipeline_layout_; }

  std::unique_ptr<BindingData> CreateMaterialBindingData() override;
  std::unique_ptr<BindingData> CreateInstanceBindingData() override;

 private:
  VulkanRenderPipeline() = default;

  bool Init(VulkanBackend* backend, VulkanSceneType* scene_type,
            const VertexType* vertex_type, absl::Span<const Binding> bindings,
            VulkanShaderCode* vertex_shader, VulkanShaderCode* fragment_shader,
            const MaterialConfig& config, vk::RenderPass render_pass);

  VulkanBackend* backend_ = nullptr;
  std::unique_ptr<VulkanBindingDataFactory> material_data_factory_;
  std::unique_ptr<VulkanBindingDataFactory> instance_data_factory_;
  vk::PipelineLayout pipeline_layout_;
  vk::Pipeline pipeline_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_RENDER_PIPELINE_H_
