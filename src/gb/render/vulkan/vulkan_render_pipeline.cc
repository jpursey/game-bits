// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_render_pipeline.h"

#include "gb/render/material_config.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_binding_data_factory.h"
#include "gb/render/vulkan/vulkan_scene_type.h"
#include "gb/render/vulkan/vulkan_shader_code.h"

namespace gb {

std::unique_ptr<VulkanRenderPipeline> VulkanRenderPipeline::Create(
    VulkanInternal, VulkanBackend* backend, VulkanSceneType* scene_type,
    const VertexType* vertex_type, absl::Span<const Binding> bindings,
    VulkanShaderCode* vertex_shader, VulkanShaderCode* fragment_shader,
    const MaterialConfig& config, vk::RenderPass render_pass) {
  auto pipeline = absl::WrapUnique(new VulkanRenderPipeline());
  if (!pipeline->Init(backend, scene_type, vertex_type, bindings, vertex_shader,
                      fragment_shader, config, render_pass)) {
    return nullptr;
  }
  return pipeline;
}

VulkanRenderPipeline::~VulkanRenderPipeline() {
  auto* gc = backend_->GetGarbageCollector();
  gc->Dispose(pipeline_);
  gc->Dispose(pipeline_layout_);
}

bool VulkanRenderPipeline::Init(
    VulkanBackend* backend, VulkanSceneType* scene_type,
    const VertexType* vertex_type, absl::Span<const Binding> bindings,
    VulkanShaderCode* vertex_shader, VulkanShaderCode* fragment_shader,
    const MaterialConfig& config, vk::RenderPass render_pass) {
  backend_ = backend;
  auto device = backend_->GetDevice();

  std::vector<Binding> material_bindings;
  std::vector<Binding> instance_bindings;
  for (const Binding& binding : bindings) {
    if (binding.set == BindingSet::kMaterial) {
      material_bindings.push_back(binding);
    } else if (binding.set == BindingSet::kInstance) {
      instance_bindings.push_back(binding);
    }
  }

  material_data_factory_ = VulkanBindingDataFactory::Create(
      {}, backend_, kMaxMaterialsPerGroup, material_bindings);
  if (material_data_factory_ == nullptr) {
    return false;
  }
  instance_data_factory_ = VulkanBindingDataFactory::Create(
      {}, backend_, kMaxInstancesPerGroup, instance_bindings);
  if (instance_data_factory_ == nullptr) {
    return false;
  }

  vk::PipelineShaderStageCreateInfo stages[] = {
      vk::PipelineShaderStageCreateInfo()
          .setStage(vk::ShaderStageFlagBits::eVertex)
          .setModule(vertex_shader->Get())
          .setPName("main"),
      vk::PipelineShaderStageCreateInfo()
          .setStage(vk::ShaderStageFlagBits::eFragment)
          .setModule(fragment_shader->Get())
          .setPName("main"),
  };

  auto vertex_binding = vk::VertexInputBindingDescription()
                            .setBinding(0)
                            .setStride(vertex_type->GetSize())
                            .setInputRate(vk::VertexInputRate::eVertex);

  auto attributes = vertex_type->GetAttributes();
  std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
  vertex_attributes.reserve(attributes.size());
  uint32_t location = 0;
  uint32_t offset = 0;
  for (const auto& attribute : attributes) {
    vk::Format format = vk::Format::eUndefined;
    uint32_t size = 0;
    switch (attribute) {
      case ShaderValue::kFloat:
        format = vk::Format::eR32Sfloat;
        size = 4;
        break;
      case ShaderValue::kVec2:
        format = vk::Format::eR32G32Sfloat;
        size = 8;
        break;
      case ShaderValue::kVec3:
        format = vk::Format::eR32G32B32Sfloat;
        size = 12;
        break;
      case ShaderValue::kVec4:
        format = vk::Format::eR32G32B32A32Sfloat;
        size = 16;
        break;
      case ShaderValue::kColor:
        format = vk::Format::eR8G8B8A8Unorm;
        size = 4;
        break;
      default:
        LOG(ERROR) << "Unhandled vertex attribute type";
        return false;
    }
    vertex_attributes.emplace_back()
        .setBinding(0)
        .setLocation(location)
        .setFormat(format)
        .setOffset(offset);
    location += 1;
    offset += size;
  }

  auto vertex_input_info =
      vk::PipelineVertexInputStateCreateInfo()
          .setVertexBindingDescriptionCount(1)
          .setPVertexBindingDescriptions(&vertex_binding)
          .setVertexAttributeDescriptionCount(
              static_cast<uint32_t>(vertex_attributes.size()))
          .setPVertexAttributeDescriptions(vertex_attributes.data());

  auto input_assembly = vk::PipelineInputAssemblyStateCreateInfo().setTopology(
      vk::PrimitiveTopology::eTriangleList);

  // We have to set a viewport and scissor when constructing the pipeline, but
  // the values are unimportant, as it is overridden during rendering.
  auto viewport = vk::Viewport(0.0f, 0.0f, 640.0f, 480.f, 0.0f, 1.0f);
  vk::Rect2D scissor = {{0, 0}, {640, 480}};
  auto viewport_state = vk::PipelineViewportStateCreateInfo()
                            .setViewportCount(1)
                            .setPViewports(&viewport)
                            .setScissorCount(1)
                            .setPScissors(&scissor);

  vk::CullModeFlags cull_mode = {};
  if (config.cull_mode == CullMode::kBack) {
    cull_mode = vk::CullModeFlagBits::eBack;
  } else if (config.cull_mode == CullMode::kFront) {
    cull_mode = vk::CullModeFlagBits::eFront;
  }
  auto rasterizer = vk::PipelineRasterizationStateCreateInfo()
                        .setPolygonMode(vk::PolygonMode::eFill)
                        .setLineWidth(1.0f)
                        .setCullMode(cull_mode)
                        .setFrontFace(vk::FrontFace::eCounterClockwise);

  auto multisampling =
      vk::PipelineMultisampleStateCreateInfo().setRasterizationSamples(
          backend_->GetMsaaSampleCount());

  auto depth_stencil =
      vk::PipelineDepthStencilStateCreateInfo()
          .setDepthTestEnable(config.depth_mode == DepthMode::kTest ||
                              config.depth_mode == DepthMode::kTestAndWrite)
          .setDepthWriteEnable(config.depth_mode == DepthMode::kWrite ||
                               config.depth_mode == DepthMode::kTestAndWrite)
          .setDepthCompareOp(vk::CompareOp::eLess)
          .setDepthBoundsTestEnable(VK_FALSE)
          .setMinDepthBounds(0.0f)
          .setMaxDepthBounds(1.0f)
          .setStencilTestEnable(VK_FALSE)
          .setFront({})
          .setBack({});

  auto color_blend_attachment =
      vk::PipelineColorBlendAttachmentState()
          .setColorWriteMask(
              vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
          .setBlendEnable(VK_TRUE)
          .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
          .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
          .setColorBlendOp(vk::BlendOp::eAdd)
          .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
          .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
          .setAlphaBlendOp(vk::BlendOp::eAdd);

  auto color_blending = vk::PipelineColorBlendStateCreateInfo()
                            .setAttachmentCount(1)
                            .setPAttachments(&color_blend_attachment);

  vk::DynamicState dynamic_states[] = {vk::DynamicState::eViewport,
                                       vk::DynamicState::eScissor};
  auto dynamic_state = vk::PipelineDynamicStateCreateInfo()
                           .setDynamicStateCount(ABSL_ARRAYSIZE(dynamic_states))
                           .setPDynamicStates(dynamic_states);

  vk::DescriptorSetLayout descriptor_set_layouts[] = {
      scene_type->GetLayout(),
      material_data_factory_->GetLayout(),
      instance_data_factory_->GetLayout(),
  };

  auto [create_layout_result, pipeline_layout] = device.createPipelineLayout(
      vk::PipelineLayoutCreateInfo()
          .setSetLayoutCount(ABSL_ARRAYSIZE(descriptor_set_layouts))
          .setPSetLayouts(descriptor_set_layouts));
  if (create_layout_result != vk::Result::eSuccess) {
    LOG(ERROR) << "Failed to create pipeline layout";
    return false;
  }
  pipeline_layout_ = pipeline_layout;

  auto [create_pipeline_result, pipeline] = device.createGraphicsPipeline(
      {}, vk::GraphicsPipelineCreateInfo()
              .setStageCount(ABSL_ARRAYSIZE(stages))
              .setPStages(stages)
              .setPVertexInputState(&vertex_input_info)
              .setPInputAssemblyState(&input_assembly)
              .setPViewportState(&viewport_state)
              .setPRasterizationState(&rasterizer)
              .setPMultisampleState(&multisampling)
              .setPDepthStencilState(&depth_stencil)
              .setPColorBlendState(&color_blending)
              .setLayout(pipeline_layout_)
              .setRenderPass(render_pass)
              .setSubpass(0)
              .setPDynamicState(&dynamic_state));
  if (create_pipeline_result != vk::Result::eSuccess) {
    LOG(ERROR) << "Failed to create pipeline";
    return false;
  }
  pipeline_ = pipeline;

  return true;
}

std::unique_ptr<BindingData> VulkanRenderPipeline::CreateMaterialBindingData() {
  return material_data_factory_->NewBindingData(this, BindingSet::kMaterial);
}

std::unique_ptr<BindingData> VulkanRenderPipeline::CreateInstanceBindingData() {
  return instance_data_factory_->NewBindingData(this, BindingSet::kInstance);
}

}  // namespace gb
