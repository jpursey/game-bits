// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_binding_data_factory.h"

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "gb/render/vulkan/vulkan_backend.h"

namespace gb {

namespace {

inline int Align(int value, int alignment) {
  const size_t unsigned_alignment = static_cast<size_t>(alignment);
  return static_cast<int>(
      (static_cast<size_t>(value) + unsigned_alignment - 1) &
      ~(unsigned_alignment - 1));
}

}  // namespace

std::unique_ptr<VulkanBindingDataFactory> VulkanBindingDataFactory::Create(
    VulkanInternal, VulkanBackend* backend, int max_buffer_slots,
    absl::Span<const Binding> bindings) {
  std::unique_ptr<VulkanDescriptorPool> descriptor_pool;
  descriptor_pool =
      VulkanDescriptorPool::Create({}, backend, max_buffer_slots, bindings);
  if (descriptor_pool == nullptr) {
    return nullptr;
  }

  auto factory = absl::WrapUnique(new VulkanBindingDataFactory(
      backend, max_buffer_slots, std::move(descriptor_pool), bindings));
  if (!factory->AddBindingGroup()) {
    return nullptr;
  }
  return factory;
}

VulkanBindingDataFactory::VulkanBindingDataFactory(
    VulkanBackend* backend, int max_buffer_slots,
    std::unique_ptr<VulkanDescriptorPool> descriptor_pool,
    absl::Span<const Binding> bindings)
    : WeakScope(this),
      backend_(backend),
      bindings_(bindings.begin(), bindings.end()),
      descriptor_pool_(std::move(descriptor_pool)) {
  int max_constants_size = 1;
  for (const auto& binding : bindings_) {
    if (binding.index >= binding_size_) {
      binding_size_ = binding.index + 1;
    }
    if (binding.binding_type == BindingType::kConstants) {
      ++buffer_count_;
      max_constants_size =
          std::max(max_constants_size, binding.constants_type->GetSize());
    }
  }

  if (buffer_count_ > 0) {
    const auto& limits = backend_->GetPhysicalDeviceProperties().limits;
    int max_buffer_size = static_cast<int>(limits.maxUniformBufferRange);
    if (max_buffer_size < 0) {
      max_buffer_size = std::numeric_limits<int>::max();
    }
    buffer_value_alignment_ =
        static_cast<int>(limits.minUniformBufferOffsetAlignment);
    max_constants_size = Align(max_constants_size, buffer_value_alignment_);
    buffer_slots_ =
        std::min(max_buffer_slots, max_buffer_size / max_constants_size);
  }
}

VulkanBindingDataFactory::~VulkanBindingDataFactory() {
  InvalidateWeakPtrs();
  binding_groups_.clear();
  descriptor_pool_.reset();
}

std::unique_ptr<VulkanBindingData> VulkanBindingDataFactory::NewBindingData(
    RenderPipeline* pipeline, BindingSet set) {
  mutex_.Lock();
  if (free_index_ == -1) {
    if (!AddBindingGroup()) {
      mutex_.Unlock();
      return nullptr;
    }
  }

  if (binding_size_ == 0) {
    // Empty binding data.
    mutex_.Unlock();
    return std::make_unique<VulkanBindingData>(VulkanInternal{}, this, pipeline,
                                               set);
  }

  std::array<vk::DescriptorSet, kMaxFramesInFlight> descriptor_sets;
  for (auto& descriptor_set : descriptor_sets) {
    descriptor_set = descriptor_pool_->NewSet();
    if (!descriptor_set) {
      mutex_.Unlock();
      return nullptr;
    }
  }

  if (buffer_count_ == 0) {
    // Bufferless binding data.
    mutex_.Unlock();
    return std::make_unique<VulkanBindingData>(
        VulkanInternal{}, this, pipeline, set,
        binding_groups_.front().binding_data, descriptor_sets);
  }

  const int group = free_index_ / buffer_slots_;
  const int index = free_index_ % buffer_slots_;
  auto& binding_group = binding_groups_[group];
  free_index_ = binding_group.buffer_free[index];
  binding_group.buffer_free[index] = -1;

  mutex_.Unlock();
  return std::make_unique<VulkanBindingData>(
      VulkanInternal{}, this, pipeline, set, group, index,
      binding_group.binding_data, descriptor_sets);
}

void VulkanBindingDataFactory::DisposeBindingData(
    int index, int offset, absl::Span<vk::DescriptorSet> descriptor_sets) {
  absl::MutexLock lock(&mutex_);
  if (buffer_count_ > 0) {
    binding_groups_[index].buffer_free[offset] = free_index_;
    free_index_ = index * buffer_slots_ + offset;
  }
  for (auto descriptor_set : descriptor_sets) {
    descriptor_pool_->DisposeSet(descriptor_set);
  }
}

bool VulkanBindingDataFactory::AddBindingGroup() {
  RENDER_ASSERT(free_index_ == -1);

  std::vector<std::unique_ptr<VulkanRenderBuffer>> buffers;
  buffers.reserve(buffer_count_);

  std::vector<VulkanBindingData::DataItem> binding_data(binding_size_);
  for (const auto& binding : bindings_) {
    auto& item = binding_data[binding.index];
    item.binding_type = binding.binding_type;
    if (binding.binding_type == BindingType::kTexture) {
      auto& info = item.texture;
      info.texture = nullptr;
      for (auto& bound : info.bound) {
        bound = false;
      }
    } else if (binding.binding_type == BindingType::kTextureArray) {
      auto& info = item.texture_array;
      info.texture_array = nullptr;
      for (auto& bound : info.bound) {
        bound = false;
      }
    } else if (binding.binding_type == BindingType::kConstants) {
      const int value_size = binding.constants_type->GetSize();
      auto buffer = VulkanRenderBuffer::Create(
          {}, backend_, VulkanBufferType::kUniform, binding.volatility,
          value_size, buffer_slots_,
          Align(value_size, buffer_value_alignment_));
      buffer->Resize(buffer_slots_);
      if (buffer == nullptr) {
        LOG(ERROR)
            << "Failed to create binding group, due to buffer creation failure";
        return false;
      }

      auto& info = item.constants;
      info.buffer = buffer.get();
      info.type = binding.constants_type;
      for (auto& bound : info.bound) {
        bound = -1;
      }
      buffers.emplace_back(std::move(buffer));
    }
  }

  int free_index = static_cast<int>(binding_groups_.size()) * buffer_slots_;
  std::vector<int> buffer_free(buffer_slots_, -1);
  for (int i = 0; i < buffer_slots_ - 1; ++i) {
    buffer_free[i] = free_index + i + 1;
  }

  auto& binding_group = binding_groups_.emplace_back();
  binding_group.binding_data = std::move(binding_data);
  binding_group.buffers = std::move(buffers);
  binding_group.buffer_free = std::move(buffer_free);
  free_index_ = free_index;
  return true;
}

}  // namespace gb
