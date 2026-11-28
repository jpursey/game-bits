// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_binding_data.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "gb/render/webgpu/webgpu_format.h"

namespace gb {

namespace {

wgpu::ShaderStage GetShaderStages(ShaderTypes types) {
  wgpu::ShaderStage stages = wgpu::ShaderStage::None;
  if (types.IsSet(ShaderType::kVertex)) {
    stages |= wgpu::ShaderStage::Vertex;
  }
  if (types.IsSet(ShaderType::kFragment)) {
    stages |= wgpu::ShaderStage::Fragment;
  }
  return stages;
}

}  // namespace

std::shared_ptr<const WebGpuBindingSetLayout> WebGpuBindingSetLayout::Create(
    WebGpuBackend* backend, BindingSet set,
    absl::Span<const Binding> bindings) {
  auto layout = std::make_shared<WebGpuBindingSetLayout>();
  layout->set = set;
  const int64_t offset_alignment =
      backend->GetLimits().minUniformBufferOffsetAlignment;
  std::vector<wgpu::BindGroupLayoutEntry> entries;
  for (const Binding& binding : bindings) {
    if (binding.set != set) {
      continue;
    }
    if (binding.binding_type != BindingType::kConstants) {
      LOG(ERROR) << "The WebGPU backend doesn't support binding type "
                 << static_cast<int>(binding.binding_type) << " yet";
      return nullptr;
    }
    if (binding.index > kMaxWebGpuBindingIndex) {
      LOG(ERROR) << "Binding index " << binding.index
                 << " is beyond the WebGPU backend's limit of "
                 << kMaxWebGpuBindingIndex;
      return nullptr;
    }
    if (binding.index >= static_cast<int>(layout->slots.size())) {
      layout->slots.resize(binding.index + 1);
    }
    Slot& slot = layout->slots[binding.index];
    slot.binding_type = binding.binding_type;
    slot.data_type = binding.data_type;

    // A uniform binding is bound in whole 16-byte rows, as WGSL lays it out.
    slot.offset = AlignUp(layout->buffer_size, offset_alignment);
    slot.size = AlignUp(binding.data_type->GetSize(), 16);
    layout->buffer_size = slot.offset + slot.size;

    wgpu::BindGroupLayoutEntry& entry = entries.emplace_back();
    entry.binding = static_cast<uint32_t>(
        GetWgslBinding(binding.index, WgslBindingKind::kResource));
    entry.visibility = GetShaderStages(binding.shader_types);
    entry.buffer.type = wgpu::BufferBindingType::Uniform;
    entry.buffer.minBindingSize = static_cast<uint64_t>(slot.size);
  }

  wgpu::BindGroupLayoutDescriptor descriptor;
  descriptor.entryCount = entries.size();
  descriptor.entries = entries.data();
  layout->layout = backend->GetDevice().CreateBindGroupLayout(&descriptor);
  return layout;
}

std::unique_ptr<WebGpuBindingData> WebGpuBindingData::Create(
    WebGpuBackend* backend, RenderPipeline* pipeline,
    std::shared_ptr<const WebGpuBindingSetLayout> layout) {
  wgpu::Buffer buffer;
  std::vector<wgpu::BindGroupEntry> entries;
  if (layout->buffer_size > 0) {
    wgpu::BufferDescriptor descriptor;
    descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    descriptor.size = static_cast<uint64_t>(layout->buffer_size);
    buffer = backend->GetDevice().CreateBuffer(&descriptor);
  }
  const int slot_count = static_cast<int>(layout->slots.size());
  for (int index = 0; index < slot_count; ++index) {
    const WebGpuBindingSetLayout::Slot& slot = layout->slots[index];
    if (slot.binding_type != BindingType::kConstants) {
      continue;
    }
    wgpu::BindGroupEntry& entry = entries.emplace_back();
    entry.binding = static_cast<uint32_t>(
        GetWgslBinding(index, WgslBindingKind::kResource));
    entry.buffer = buffer;
    entry.offset = static_cast<uint64_t>(slot.offset);
    entry.size = static_cast<uint64_t>(slot.size);
  }

  wgpu::BindGroupDescriptor descriptor;
  descriptor.layout = layout->layout;
  descriptor.entryCount = entries.size();
  descriptor.entries = entries.data();
  wgpu::BindGroup bind_group = backend->GetDevice().CreateBindGroup(&descriptor);
  if (bind_group == nullptr) {
    LOG(ERROR) << "Failed to create a WebGPU bind group";
    return nullptr;
  }
  return absl::WrapUnique(new WebGpuBindingData(backend, pipeline,
                                                std::move(layout),
                                                std::move(buffer),
                                                std::move(bind_group)));
}

WebGpuBindingData::WebGpuBindingData(
    WebGpuBackend* backend, RenderPipeline* pipeline,
    std::shared_ptr<const WebGpuBindingSetLayout> layout, wgpu::Buffer buffer,
    wgpu::BindGroup bind_group)
    : BindingData(pipeline, layout->set),
      WebGpuPendingWrite(backend),
      layout_(std::move(layout)),
      buffer_(std::move(buffer)),
      bind_group_(std::move(bind_group)),
      data_(layout_->buffer_size) {
  GetBackend()->AddAllocatedBytes(layout_->buffer_size);
}

WebGpuBindingData::~WebGpuBindingData() {
  if (buffer_ != nullptr) {
    buffer_.Destroy();
  }
  GetBackend()->AddAllocatedBytes(-layout_->buffer_size);
}

bool WebGpuBindingData::Validate(int index, TypeKey* type) const {
  if (index < 0 || index >= static_cast<int>(layout_->slots.size())) {
    return false;
  }
  const WebGpuBindingSetLayout::Slot& slot = layout_->slots[index];
  switch (slot.binding_type) {
    case BindingType::kNone:
      return false;
    case BindingType::kConstants:
      return slot.data_type->GetType() == type;
    case BindingType::kTexture:
      return type == TypeKey::Get<Texture*>();
    case BindingType::kTextureArray:
      return type == TypeKey::Get<TextureArray*>();
    case BindingType::kVolumeTexture:
      return type == TypeKey::Get<VolumeTexture*>();
    case BindingType::kStorageBuffer:
      return type == TypeKey::Get<StorageBuffer*>();
  }
  return false;
}

bool WebGpuBindingData::ValidateDataType(int index,
                                         const RenderDataType* type) const {
  if (index < 0 || index >= static_cast<int>(layout_->slots.size())) {
    return false;
  }
  const WebGpuBindingSetLayout::Slot& slot = layout_->slots[index];
  return slot.binding_type == BindingType::kStorageBuffer &&
         slot.data_type->GetType() == type->GetType();
}

void WebGpuBindingData::DoSet(int index, const void* value) {
  // Only constants bindings can be made so far (see WebGpuBindingDataFactory).
  const WebGpuBindingSetLayout::Slot& slot = layout_->slots[index];
  if (slot.binding_type != BindingType::kConstants) {
    return;
  }
  const int64_t size = slot.data_type->GetSize();
  std::memcpy(data_.data() + slot.offset, value, size);
  const Range range = {slot.offset, slot.offset + size};
  if (!pending_.has_value()) {
    pending_ = range;
    MarkPendingWrite();
    return;
  }
  pending_->begin = std::min(pending_->begin, range.begin);
  pending_->end = std::max(pending_->end, range.end);
}

void WebGpuBindingData::DoGet(int index, void* value) const {
  const WebGpuBindingSetLayout::Slot& slot = layout_->slots[index];
  if (slot.binding_type != BindingType::kConstants) {
    return;
  }
  std::memcpy(value, data_.data() + slot.offset, slot.data_type->GetSize());
}

void WebGpuBindingData::DoGetDependencies(ResourceDependencyList*) const {}

void WebGpuBindingData::WritePending(const wgpu::Queue& queue,
                                     const wgpu::CommandEncoder&) {
  if (!pending_.has_value()) {
    return;
  }

  // Each slot is 256-byte aligned and sized to 16 bytes, so whole words are
  // always within the buffer.
  const int64_t begin = pending_->begin / 4 * 4;
  const int64_t end = AlignUp(pending_->end, 4);
  queue.WriteBuffer(buffer_, static_cast<uint64_t>(begin),
                    data_.data() + begin, static_cast<size_t>(end - begin));
  pending_.reset();
}

}  // namespace gb
