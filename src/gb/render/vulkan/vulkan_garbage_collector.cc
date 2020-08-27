// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_garbage_collector.h"

namespace gb {

void VulkanGarbageCollector::Collect(vk::Device device,
                                     VmaAllocator allocator) {
  for (const Item& item : garbage_) {
    item.callback(device, item.handle);
    if (item.allocation) {
      vmaFreeMemory(allocator, item.allocation);
    }
  }
  garbage_.clear();
}

void VulkanGarbageCollector::DisposeBuffer(vk::Device device,
                                           VulkanHandle handle) {
  device.destroyBuffer(static_cast<VkBuffer>(handle));
}

void VulkanGarbageCollector::DisposeDescriptorPool(vk::Device device,
                                                   VulkanHandle handle) {
  device.destroyDescriptorPool(static_cast<VkDescriptorPool>(handle));
}

void VulkanGarbageCollector::DisposeDescriptorSetLayout(vk::Device device,
                                                        VulkanHandle handle) {
  device.destroyDescriptorSetLayout(static_cast<VkDescriptorSetLayout>(handle));
}

void VulkanGarbageCollector::DisposeImage(vk::Device device,
                                          VulkanHandle handle) {
  device.destroyImage(static_cast<VkImage>(handle));
}

void VulkanGarbageCollector::DisposeImageView(vk::Device device,
                                              VulkanHandle handle) {
  device.destroyImageView(static_cast<VkImageView>(handle));
}

void VulkanGarbageCollector::DisposePipeline(vk::Device device,
                                             VulkanHandle handle) {
  device.destroyPipeline(static_cast<VkPipeline>(handle));
}

void VulkanGarbageCollector::DisposePipelineLayout(vk::Device device,
                                                   VulkanHandle handle) {
  device.destroyPipelineLayout(static_cast<VkPipelineLayout>(handle));
}

void VulkanGarbageCollector::DisposeShaderModule(vk::Device device,
                                                 VulkanHandle handle) {
  device.destroyShaderModule(static_cast<VkShaderModule>(handle));
}

}  // namespace gb
