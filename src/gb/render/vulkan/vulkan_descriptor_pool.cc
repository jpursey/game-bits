// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_descriptor_pool.h"

#include "absl/memory/memory.h"
#include "gb/render/vulkan/vulkan_backend.h"

namespace gb {

std::unique_ptr<VulkanDescriptorPool> VulkanDescriptorPool::Create(
    VulkanInternal, VulkanBackend* backend, int init_pool_size,
    absl::Span<const Binding> bindings) {
  auto device = backend->GetDevice();

  BindingCounts counts = {};
  std::vector<vk::DescriptorSetLayoutBinding> layout_bindings;
  layout_bindings.reserve(bindings.size());
  for (const auto& binding : bindings) {
    if (binding.binding_type == BindingType::kNone) {
      continue;
    }

    vk::DescriptorType type;
    if (binding.binding_type == BindingType::kTexture ||
        binding.binding_type == BindingType::kTextureArray) {
      counts.texture += 1;
      type = vk::DescriptorType::eCombinedImageSampler;
    } else {
      counts.dynamic_uniform += 1;
      type = vk::DescriptorType::eUniformBufferDynamic;
    }

    vk::ShaderStageFlags shader_flags;
    if (binding.shader_types.IsSet(ShaderType::kVertex)) {
      shader_flags |= vk::ShaderStageFlagBits::eVertex;
    }
    if (binding.shader_types.IsSet(ShaderType::kFragment)) {
      shader_flags |= vk::ShaderStageFlagBits::eFragment;
    }

    layout_bindings.emplace_back()
        .setBinding(binding.index)
        .setDescriptorType(type)
        .setDescriptorCount(1)
        .setStageFlags(shader_flags);
  }
  auto [result, layout] = device.createDescriptorSetLayout(
      vk::DescriptorSetLayoutCreateInfo()
          .setBindingCount(static_cast<uint32_t>(layout_bindings.size()))
          .setPBindings(layout_bindings.data()));
  if (result != vk::Result::eSuccess) {
    LOG(ERROR) << "Failed to create descriptor set layout";
    return nullptr;
  }

  auto pool = absl::WrapUnique(
      new VulkanDescriptorPool(backend, counts, layout, init_pool_size));
  if (!bindings.empty() && !pool->NewPool()) {
    return nullptr;
  }
  return pool;
}

VulkanDescriptorPool::VulkanDescriptorPool(VulkanBackend* backend,
                                           BindingCounts counts,
                                           vk::DescriptorSetLayout layout,
                                           int init_pool_size)
    : backend_(backend),
      counts_(counts),
      layout_(layout),
      pool_size_(init_pool_size),
      available_sets_(100) {}

VulkanDescriptorPool::~VulkanDescriptorPool() {
  auto* gc = backend_->GetGarbageCollector();
  for (auto pool : pools_) {
    gc->Dispose(pool);
  }
  gc->Dispose(layout_);
}

bool VulkanDescriptorPool::NewPool() {
  absl::InlinedVector<vk::DescriptorPoolSize, 3> sizes;
  if (counts_.texture > 0) {
    sizes.emplace_back(vk::DescriptorType::eCombinedImageSampler,
                       static_cast<uint32_t>(counts_.texture * pool_size_));
  }
  if (counts_.dynamic_uniform > 0) {
    sizes.emplace_back(
        vk::DescriptorType::eUniformBufferDynamic,
        static_cast<uint32_t>(counts_.dynamic_uniform * pool_size_));
  }
  auto device = backend_->GetDevice();
  auto [result, pool] = device.createDescriptorPool(
      vk::DescriptorPoolCreateInfo()
          .setPoolSizeCount(static_cast<uint32_t>(sizes.size()))
          .setPPoolSizes(sizes.data())
          .setMaxSets(pool_size_));
  if (result != vk::Result::eSuccess) {
    LOG(ERROR) << "Failed to create descriptor set pool";
    return false;
  }
  pools_.emplace_back(pool);
  unallocated_ += pool_size_;
  pool_size_ *= 2;
  return true;
}

vk::DescriptorSet VulkanDescriptorPool::NewSet() {
  if (!available_sets_.empty()) {
    auto& available_set = available_sets_.front();
    if (available_set.dispose_frame > backend_->GetFrame() + 1) {
      auto new_set = available_set.set;
      available_sets_.pop();
      return new_set;
    }
  }
  if (unallocated_ == 0) {
    if (!NewPool()) {
      return {};
    }
  }
  auto device = backend_->GetDevice();
  auto [result, new_sets] =
      device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo()
                                        .setDescriptorPool(pools_.back())
                                        .setDescriptorSetCount(1)
                                        .setPSetLayouts(&layout_));
  if (result != vk::Result::eSuccess) {
    LOG(ERROR)
        << "Failed to allocate descriptor set from pool with available space";
    return {};
  }
  --unallocated_;
  return new_sets.front();
}

void VulkanDescriptorPool::DisposeSet(vk::DescriptorSet set) {
  available_sets_.emplace(AvailableSet{backend_->GetFrame(), set});
}

}  // namespace gb
