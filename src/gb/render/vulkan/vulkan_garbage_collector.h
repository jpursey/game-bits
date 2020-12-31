// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef VULKAN_GARBAGE_COLLECTOR_H_
#define VULKAN_GARBAGE_COLLECTOR_H_

#include <type_traits>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "gb/render/vulkan/vulkan_allocator.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// This class manages safe destruction of Vulkan resources once they are
// guaranteed to no longer be used by any code.
//
// This class is thread-safe.
class VulkanGarbageCollector {
 public:
  void Dispose(VkBuffer buffer, VmaAllocation allocation = VK_NULL_HANDLE);
  void Dispose(VkDescriptorPool descriptor_pool);
  void Dispose(VkDescriptorSetLayout descriptor_set_layout);
  void Dispose(VkImage image, VmaAllocation allocation = VK_NULL_HANDLE);
  void Dispose(VkImageView image_view);
  void Dispose(VkPipeline pipeline);
  void Dispose(VkPipelineLayout pipeline_layout);
  void Dispose(VkShaderModule shader);

  void Collect(vk::Device device, VmaAllocator allocator);

 private:
  using VulkanHandle =
      std::conditional_t<std::is_same_v<uint64_t, VkBuffer>, uint64_t, void*>;
  using DisposeCallback = void (*)(vk::Device device, VulkanHandle handle);

  struct Item {
    DisposeCallback callback;
    VulkanHandle handle;
    VmaAllocation allocation;
  };

  static void DisposeBuffer(vk::Device device, VulkanHandle handle);
  static void DisposeDescriptorPool(vk::Device device, VulkanHandle handle);
  static void DisposeDescriptorSetLayout(vk::Device device,
                                         VulkanHandle handle);
  static void DisposeImage(vk::Device device, VulkanHandle handle);
  static void DisposeImageView(vk::Device device, VulkanHandle handle);
  static void DisposePipeline(vk::Device device, VulkanHandle handle);
  static void DisposePipelineLayout(vk::Device device, VulkanHandle handle);
  static void DisposeShaderModule(vk::Device device, VulkanHandle handle);

  mutable absl::Mutex mutex_;
  std::vector<Item> garbage_ ABSL_GUARDED_BY(mutex_);
};

inline void VulkanGarbageCollector::Dispose(VkBuffer buffer,
                                            VmaAllocation allocation) {
  if (buffer) {
    absl::MutexLock lock(&mutex_);
    garbage_.push_back({DisposeBuffer, buffer, allocation});
  }
}

inline void VulkanGarbageCollector::Dispose(VkDescriptorPool descriptor_pool) {
  if (descriptor_pool) {
    absl::MutexLock lock(&mutex_);
    garbage_.push_back({DisposeDescriptorPool, descriptor_pool});
  }
}

inline void VulkanGarbageCollector::Dispose(
    VkDescriptorSetLayout descriptor_set_layout) {
  if (descriptor_set_layout) {
    absl::MutexLock lock(&mutex_);
    garbage_.push_back({DisposeDescriptorSetLayout, descriptor_set_layout});
  }
}

inline void VulkanGarbageCollector::Dispose(VkImage image,
                                            VmaAllocation allocation) {
  if (image) {
    absl::MutexLock lock(&mutex_);
    garbage_.push_back({DisposeImage, image, allocation});
  }
}

inline void VulkanGarbageCollector::Dispose(VkImageView image_view) {
  if (image_view) {
    absl::MutexLock lock(&mutex_);
    garbage_.push_back({DisposeImageView, image_view});
  }
}

inline void VulkanGarbageCollector::Dispose(VkPipeline pipeline) {
  if (pipeline) {
    absl::MutexLock lock(&mutex_);
    garbage_.push_back({DisposePipeline, pipeline});
  }
}

inline void VulkanGarbageCollector::Dispose(VkPipelineLayout pipeline_layout) {
  if (pipeline_layout) {
    absl::MutexLock lock(&mutex_);
    garbage_.push_back({DisposePipelineLayout, pipeline_layout});
  }
}

inline void VulkanGarbageCollector::Dispose(VkShaderModule shader) {
  if (shader) {
    absl::MutexLock lock(&mutex_);
    garbage_.push_back({DisposeShaderModule, shader});
  }
}

}  // namespace gb

#endif VULKAN_GARBAGE_COLLECTOR_H_
