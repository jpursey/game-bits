// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_render_pipeline.h"

#include <cmath>
#include <utility>

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "gb/render/webgpu/webgpu_format.h"

namespace gb {

namespace {

wgpu::CompareFunction GetCompareFunction(DepthTest test) {
  switch (test) {
    case DepthTest::kLess:
      return wgpu::CompareFunction::Less;
    case DepthTest::kLessOrEqual:
      return wgpu::CompareFunction::LessEqual;
    case DepthTest::kEqual:
      return wgpu::CompareFunction::Equal;
    case DepthTest::kGreaterOrEqual:
      return wgpu::CompareFunction::GreaterEqual;
    case DepthTest::kGreater:
      return wgpu::CompareFunction::Greater;
  }
  return wgpu::CompareFunction::Less;
}

wgpu::ColorWriteMask GetWriteMask(ColorChannels channels) {
  wgpu::ColorWriteMask mask = wgpu::ColorWriteMask::None;
  if (channels.IsSet(ColorChannel::kRed)) {
    mask |= wgpu::ColorWriteMask::Red;
  }
  if (channels.IsSet(ColorChannel::kGreen)) {
    mask |= wgpu::ColorWriteMask::Green;
  }
  if (channels.IsSet(ColorChannel::kBlue)) {
    mask |= wgpu::ColorWriteMask::Blue;
  }
  if (channels.IsSet(ColorChannel::kAlpha)) {
    mask |= wgpu::ColorWriteMask::Alpha;
  }
  return mask;
}

// The blend states match the Vulkan backend's.
wgpu::BlendState GetBlendState(BlendMode mode) {
  using Factor = wgpu::BlendFactor;
  wgpu::BlendState blend;
  switch (mode) {
    case BlendMode::kNone:
      break;
    case BlendMode::kAlpha:
      blend.color = {wgpu::BlendOperation::Add, Factor::SrcAlpha,
                     Factor::OneMinusSrcAlpha};
      blend.alpha = {wgpu::BlendOperation::Add, Factor::One, Factor::Zero};
      break;
    case BlendMode::kPremultiplied:
      blend.color = {wgpu::BlendOperation::Add, Factor::One,
                     Factor::OneMinusSrcAlpha};
      blend.alpha = {wgpu::BlendOperation::Add, Factor::One,
                     Factor::OneMinusSrcAlpha};
      break;
    case BlendMode::kAdditive:
      blend.color = {wgpu::BlendOperation::Add, Factor::SrcAlpha, Factor::One};
      blend.alpha = {wgpu::BlendOperation::Add, Factor::Zero, Factor::One};
      break;
  }
  return blend;
}

}  // namespace

std::unique_ptr<WebGpuRenderPipeline> WebGpuRenderPipeline::Create(
    WebGpuBackend* backend, WebGpuBindingLayout* binding_layout,
    const VertexType* vertex_type, absl::Span<const Binding> bindings,
    WebGpuShaderCode* vertex_shader, WebGpuShaderCode* fragment_shader,
    const MaterialConfig& config, const WebGpuPassKind& frame_kind) {
  if (config.raster_mode == RasterMode::kLine) {
    LOG(ERROR) << "WebGPU has no wireframe fill (RasterMode::kLine)";
    return nullptr;
  }
  auto pipeline = absl::WrapUnique(new WebGpuRenderPipeline(
      backend, vertex_shader, fragment_shader, config));
  if (!pipeline->Init(binding_layout, vertex_type, bindings) ||
      pipeline->Get(frame_kind) == nullptr) {
    return nullptr;
  }
  return pipeline;
}

bool WebGpuRenderPipeline::Init(WebGpuBindingLayout* binding_layout,
                                const VertexType* vertex_type,
                                absl::Span<const Binding> bindings) {
  vertex_size_ = vertex_type->GetSize();
  int location = 0;
  int offset = 0;
  for (const ShaderValue value : vertex_type->GetAttributes()) {
    const WebGpuVertexFormat format = GetWebGpuVertexFormat(value);
    if (format.format == wgpu::VertexFormat(0)) {
      LOG(ERROR) << "WebGPU has no vertex format for attribute " << location
                 << ", a 3-channel 8 or 16-bit value";
      return false;
    }
    wgpu::VertexAttribute& attribute = vertex_attributes_.emplace_back();
    attribute.format = format.format;
    attribute.offset = static_cast<uint64_t>(offset);
    attribute.shaderLocation = static_cast<uint32_t>(location);
    ++location;
    offset += format.size;
  }

  return backend_->CreateInErrorScope("a pipeline's layout", [&] {
    material_factory_ = WebGpuBindingDataFactory::Create(
        backend_, BindingSet::kMaterial, bindings);
    instance_factory_ = WebGpuBindingDataFactory::Create(
        backend_, BindingSet::kInstance, bindings);
    if (material_factory_ == nullptr || instance_factory_ == nullptr) {
      return;
    }
    const wgpu::BindGroupLayout layouts[] = {
        binding_layout->GetViewFactory()->GetBindGroupLayout(),
        material_factory_->GetBindGroupLayout(),
        instance_factory_->GetBindGroupLayout(),
    };
    wgpu::PipelineLayoutDescriptor descriptor;
    descriptor.bindGroupLayoutCount = 3;
    descriptor.bindGroupLayouts = layouts;
    pipeline_layout_ = backend_->GetDevice().CreatePipelineLayout(&descriptor);
  }) && pipeline_layout_ != nullptr;
}

wgpu::RenderPipeline WebGpuRenderPipeline::Get(const WebGpuPassKind& kind) {
  absl::MutexLock lock(&mutex_);
  auto [it, inserted] = pipelines_.try_emplace(kind);
  if (inserted) {
    it->second = CreatePipeline(kind);
  }
  return it->second;
}

wgpu::RenderPipeline WebGpuRenderPipeline::CreatePipeline(
    const WebGpuPassKind& kind) {
  wgpu::VertexBufferLayout vertex_buffer;
  vertex_buffer.stepMode = wgpu::VertexStepMode::Vertex;
  vertex_buffer.arrayStride = static_cast<uint64_t>(vertex_size_);
  vertex_buffer.attributeCount = vertex_attributes_.size();
  vertex_buffer.attributes = vertex_attributes_.data();

  wgpu::RenderPipelineDescriptor descriptor;
  descriptor.layout = pipeline_layout_;
  descriptor.vertex.module = vertex_shader_->Get();
  descriptor.vertex.entryPoint = "main";
  descriptor.vertex.bufferCount = 1;
  descriptor.vertex.buffers = &vertex_buffer;
  descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

  // gb_render's counter-clockwise front faces are clockwise once the shader
  // conversion flips y.
  descriptor.primitive.frontFace = wgpu::FrontFace::CW;
  switch (config_.cull_mode) {
    case CullMode::kNone:
      descriptor.primitive.cullMode = wgpu::CullMode::None;
      break;
    case CullMode::kFront:
      descriptor.primitive.cullMode = wgpu::CullMode::Front;
      break;
    case CullMode::kBack:
      descriptor.primitive.cullMode = wgpu::CullMode::Back;
      break;
  }
  descriptor.multisample.count = static_cast<uint32_t>(kind.sample_count);

  // Depth as the Vulkan backend does it: a read-only depth pass tests without
  // writing.
  wgpu::DepthStencilState depth;
  if (kind.depth_format != wgpu::TextureFormat::Undefined) {
    DepthMode mode = config_.depth_mode;
    if (kind.depth_read_only) {
      if (mode == DepthMode::kWrite) {
        mode = DepthMode::kNone;
      } else if (mode == DepthMode::kTestAndWrite) {
        mode = DepthMode::kTest;
      }
    }
    depth.format = kind.depth_format;
    depth.depthWriteEnabled =
        (mode == DepthMode::kWrite || mode == DepthMode::kTestAndWrite)
            ? wgpu::OptionalBool::True
            : wgpu::OptionalBool::False;
    depth.depthCompare =
        (mode == DepthMode::kTest || mode == DepthMode::kTestAndWrite)
            ? GetCompareFunction(config_.depth_test)
            : wgpu::CompareFunction::Always;
    if (mode != DepthMode::kNone) {
      depth.depthBias =
          static_cast<int32_t>(std::lround(config_.depth_bias_constant));
      depth.depthBiasSlopeScale = config_.depth_bias_slope;
    }
    descriptor.depthStencil = &depth;
  }

  const wgpu::ColorWriteMask write_mask = GetWriteMask(config_.color_channels);
  const wgpu::BlendState blend = GetBlendState(config_.blend_mode);
  wgpu::ColorTargetState target;
  target.format = kind.color_format;
  target.writeMask = write_mask;
  if (write_mask != wgpu::ColorWriteMask::None &&
      config_.blend_mode != BlendMode::kNone) {
    target.blend = &blend;
  }
  wgpu::FragmentState fragment;
  fragment.module = fragment_shader_->Get();
  fragment.entryPoint = "main";
  if (kind.color_format != wgpu::TextureFormat::Undefined) {
    fragment.targetCount = 1;
    fragment.targets = &target;
  }
  descriptor.fragment = &fragment;

  wgpu::RenderPipeline pipeline;
  if (!backend_->CreateInErrorScope("a pipeline", [&] {
        pipeline = backend_->GetDevice().CreateRenderPipeline(&descriptor);
      })) {
    return nullptr;
  }
  return pipeline;
}

std::unique_ptr<BindingData> WebGpuRenderPipeline::CreateMaterialBindingData() {
  return material_factory_->NewBindingData(this);
}

std::unique_ptr<BindingData> WebGpuRenderPipeline::CreateInstanceBindingData() {
  return instance_factory_->NewBindingData(this);
}

bool WebGpuRenderPipeline::ValidateInstanceBindingData(
    BindingData* binding_data) {
  return static_cast<WebGpuBindingData*>(binding_data)->GetFactory() ==
         instance_factory_.get();
}

}  // namespace gb
