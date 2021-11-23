// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_DESCRIPTOR_POOL_H_
#define GB_RENDER_VULKAN_VULKAN_DESCRIPTOR_POOL_H_

#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "gb/container/queue.h"
#include "gb/render/binding.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// This class manages a pool of descriptor sets.
//
// This class is used solely by VulkanBindingDataFactory to create
// VulkanBindingData.
//
// This class is thread-compatible, except as noted.
class VulkanDescriptorPool final {
 public:
  // The create function is thread-safe.
  static std::unique_ptr<VulkanDescriptorPool> Create(
      VulkanInternal, VulkanBackend* backend, int init_pool_size,
      absl::Span<const Binding> bindings);

  VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
  VulkanDescriptorPool(VulkanDescriptorPool&&) = delete;
  VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;
  VulkanDescriptorPool& operator=(VulkanDescriptorPool&&) = delete;
  ~VulkanDescriptorPool();

  // This function is thread-safe.
  const vk::DescriptorSetLayout& GetLayout() const { return layout_; }

  // These functions are thread-compatible.
  vk::DescriptorSet NewSet();
  void DisposeSet(vk::DescriptorSet set);

 private:
  struct BindingCounts {
    int texture;
    int dynamic_uniform;
  };
  struct AvailableSet {
    int dispose_frame;
    vk::DescriptorSet set;
  };

  VulkanDescriptorPool(VulkanBackend* backend, BindingCounts counts,
                       vk::DescriptorSetLayout layout, int init_pool_size);
  bool NewPool();

  VulkanBackend* const backend_;
  BindingCounts counts_;
  vk::DescriptorSetLayout layout_;
  int pool_size_ = 0;
  int unallocated_ = 0;
  std::vector<vk::DescriptorPool> pools_;
  Queue<AvailableSet> available_sets_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_DESCRIPTOR_POOL_H_
