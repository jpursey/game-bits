// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_binding_data.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"
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

//==============================================================================
// WebGpuBindingDataFactory
//==============================================================================

std::shared_ptr<WebGpuBindingDataFactory> WebGpuBindingDataFactory::Create(
    WebGpuBackend* backend, BindingSet set,
    absl::Span<const gb::Binding> bindings) {
  const int64_t offset_alignment =
      backend->GetLimits().minUniformBufferOffsetAlignment;
  std::vector<Binding> set_bindings;
  int64_t slot_size = 0;
  std::vector<wgpu::BindGroupLayoutEntry> entries;
  for (const gb::Binding& binding : bindings) {
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
    if (binding.index >= static_cast<int>(set_bindings.size())) {
      set_bindings.resize(binding.index + 1);
    }
    Binding& set_binding = set_bindings[binding.index];
    set_binding.binding_type = binding.binding_type;
    set_binding.data_type = binding.data_type;

    // A uniform binding is bound in whole 16-byte rows, as WGSL lays it out.
    set_binding.offset = AlignUp(slot_size, offset_alignment);
    set_binding.size = AlignUp(binding.data_type->GetSize(), 16);
    slot_size = set_binding.offset + set_binding.size;

    wgpu::BindGroupLayoutEntry& entry = entries.emplace_back();
    entry.binding = static_cast<uint32_t>(
        GetWgslBinding(binding.index, WgslBindingKind::kResource));
    entry.visibility = GetShaderStages(binding.shader_types);
    entry.buffer.type = wgpu::BufferBindingType::Uniform;
    entry.buffer.minBindingSize = static_cast<uint64_t>(set_binding.size);
  }

  wgpu::BindGroupLayoutDescriptor descriptor;
  descriptor.entryCount = entries.size();
  descriptor.entries = entries.data();
  wgpu::BindGroupLayout layout =
      backend->GetDevice().CreateBindGroupLayout(&descriptor);
  return std::shared_ptr<WebGpuBindingDataFactory>(new WebGpuBindingDataFactory(
      backend, set, std::move(set_bindings),
      AlignUp(slot_size, offset_alignment), std::move(layout)));
}

WebGpuBindingDataFactory::WebGpuBindingDataFactory(
    WebGpuBackend* backend, BindingSet set, std::vector<Binding> bindings,
    int64_t slot_size, wgpu::BindGroupLayout layout)
    : backend_(backend),
      set_(set),
      bindings_(std::move(bindings)),
      slot_size_(slot_size),
      slots_per_buffer_(
          slot_size == 0
              ? 0
              : static_cast<int>(std::max<int64_t>(
                    1, backend->GetLimits().maxUniformBufferBindingSize /
                           slot_size))),
      layout_(std::move(layout)) {}

WebGpuBindingDataFactory::~WebGpuBindingDataFactory() {
  for (wgpu::Buffer& buffer : buffers_) {
    buffer.Destroy();
  }
  backend_->AddAllocatedBytes(-slot_size_ * slots_per_buffer_ *
                              static_cast<int64_t>(buffers_.size()));
}

std::unique_ptr<WebGpuBindingData> WebGpuBindingDataFactory::NewBindingData(
    RenderPipeline* pipeline) {
  std::optional<Slot> slot;
  wgpu::Buffer buffer;
  if (slot_size_ > 0) {
    slot = AllocateSlot();
    if (!slot.has_value()) {
      return nullptr;
    }
    buffer = GetBuffer(*slot);
  }
  const int64_t slot_offset = slot.has_value() ? GetSlotOffset(*slot) : 0;
  std::vector<wgpu::BindGroupEntry> entries;
  const int binding_count = static_cast<int>(bindings_.size());
  for (int index = 0; index < binding_count; ++index) {
    const Binding& binding = bindings_[index];
    if (binding.binding_type != BindingType::kConstants) {
      continue;
    }
    wgpu::BindGroupEntry& entry = entries.emplace_back();
    entry.binding = static_cast<uint32_t>(
        GetWgslBinding(index, WgslBindingKind::kResource));
    entry.buffer = buffer;
    entry.offset = static_cast<uint64_t>(slot_offset + binding.offset);
    entry.size = static_cast<uint64_t>(binding.size);
  }

  wgpu::BindGroupDescriptor descriptor;
  descriptor.layout = layout_;
  descriptor.entryCount = entries.size();
  descriptor.entries = entries.data();
  wgpu::BindGroup bind_group =
      backend_->GetDevice().CreateBindGroup(&descriptor);
  if (bind_group == nullptr) {
    LOG(ERROR) << "Failed to create a WebGPU bind group";
    if (slot.has_value()) {
      FreeSlot(*slot);
    }
    return nullptr;
  }
  return absl::WrapUnique(new WebGpuBindingData(
      backend_, pipeline, shared_from_this(), slot, std::move(buffer),
      std::move(bind_group)));
}

std::optional<WebGpuBindingDataFactory::Slot>
WebGpuBindingDataFactory::AllocateSlot() {
  absl::MutexLock lock(&mutex_);
  if (free_slots_.empty()) {
    const int64_t buffer_size = slot_size_ * slots_per_buffer_;
    wgpu::BufferDescriptor descriptor;
    descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    descriptor.size = static_cast<uint64_t>(buffer_size);
    wgpu::Buffer buffer = backend_->GetDevice().CreateBuffer(&descriptor);
    if (buffer == nullptr) {
      LOG(ERROR) << "Failed to create a WebGPU uniform buffer";
      return std::nullopt;
    }
    const int buffer_index = static_cast<int>(buffers_.size());
    buffers_.push_back(std::move(buffer));
    backend_->AddAllocatedBytes(buffer_size);

    // Slots are taken from the back, so the first is used first.
    for (int i = slots_per_buffer_ - 1; i >= 0; --i) {
      free_slots_.push_back({buffer_index, i});
    }
  }
  const Slot slot = free_slots_.back();
  free_slots_.pop_back();
  return slot;
}

void WebGpuBindingDataFactory::FreeSlot(const Slot& slot) {
  absl::MutexLock lock(&mutex_);
  free_slots_.push_back(slot);
}

wgpu::Buffer WebGpuBindingDataFactory::GetBuffer(const Slot& slot) const {
  absl::MutexLock lock(&mutex_);
  return buffers_[slot.buffer_index];
}

//==============================================================================
// WebGpuBindingData
//==============================================================================

WebGpuBindingData::WebGpuBindingData(
    WebGpuBackend* backend, RenderPipeline* pipeline,
    std::shared_ptr<WebGpuBindingDataFactory> factory,
    std::optional<WebGpuBindingDataFactory::Slot> slot, wgpu::Buffer buffer,
    wgpu::BindGroup bind_group)
    : BindingData(pipeline, factory->GetSet()),
      WebGpuPendingWrite(backend),
      factory_(std::move(factory)),
      slot_(slot),
      buffer_(std::move(buffer)),
      slot_offset_(slot.has_value() ? factory_->GetSlotOffset(*slot) : 0),
      bind_group_(std::move(bind_group)),
      data_(factory_->GetSlotSize()) {}

WebGpuBindingData::~WebGpuBindingData() {
  if (slot_.has_value()) {
    factory_->FreeSlot(*slot_);
  }
}

bool WebGpuBindingData::Validate(int index, TypeKey* type) const {
  if (index < 0 || index >= static_cast<int>(factory_->GetBindings().size())) {
    return false;
  }
  const WebGpuBindingDataFactory::Binding& slot = factory_->GetBindings()[index];
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
  if (index < 0 || index >= static_cast<int>(factory_->GetBindings().size())) {
    return false;
  }
  const WebGpuBindingDataFactory::Binding& slot = factory_->GetBindings()[index];
  return slot.binding_type == BindingType::kStorageBuffer &&
         slot.data_type->GetType() == type->GetType();
}

void WebGpuBindingData::DoSet(int index, const void* value) {
  // Only constants bindings can be made so far.
  const WebGpuBindingDataFactory::Binding& slot = factory_->GetBindings()[index];
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
  const WebGpuBindingDataFactory::Binding& slot = factory_->GetBindings()[index];
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

  // Each binding is sized to 16 bytes, so whole words are always within the
  // slot.
  const int64_t begin = pending_->begin / 4 * 4;
  const int64_t end = AlignUp(pending_->end, 4);
  queue.WriteBuffer(buffer_, static_cast<uint64_t>(slot_offset_ + begin),
                    data_.data() + begin, static_cast<size_t>(end - begin));
  pending_.reset();
}

}  // namespace gb
