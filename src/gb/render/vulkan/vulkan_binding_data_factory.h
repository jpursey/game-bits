// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_BINDING_DATA_FACTORY_H_
#define GB_RENDER_VULKAN_VULKAN_BINDING_DATA_FACTORY_H_

#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "gb/base/weak_ptr.h"
#include "gb/render/binding.h"
#include "gb/render/binding_data.h"
#include "gb/render/vulkan/vulkan_binding_data.h"
#include "gb/render/vulkan/vulkan_descriptor_pool.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

class VulkanBindingDataFactory final
    : public gb::WeakScope<VulkanBindingDataFactory> {
 public:
  static std::unique_ptr<VulkanBindingDataFactory> Create(
      VulkanInternal, VulkanBackend* backend, int max_buffer_slots,
      absl::Span<const Binding> bindings);
  VulkanBindingDataFactory(const VulkanBindingDataFactory&) = delete;
  VulkanBindingDataFactory(VulkanBindingDataFactory&&) = delete;
  VulkanBindingDataFactory& operator=(const VulkanBindingDataFactory&) = delete;
  VulkanBindingDataFactory& operator=(VulkanBindingDataFactory&&) = delete;
  ~VulkanBindingDataFactory();

  vk::DescriptorSetLayout GetLayout() const {
    return descriptor_pool_->GetLayout();
  }
  int GetBufferSlots() const { return buffer_slots_; }
  int GetBufferCount() const { return buffer_count_; }

  std::unique_ptr<VulkanBindingData> NewBindingData(RenderPipeline* pipeline,
                                                    BindingSet set);
  void DisposeBindingData(int index, int offset,
                          absl::Span<vk::DescriptorSet> descriptor_sets);

 private:
  struct BindingGroup {
    std::vector<VulkanBindingData::DataItem> binding_data;
    std::vector<std::unique_ptr<VulkanRenderBuffer>> buffers;
    std::vector<int> buffer_free;
  };

  VulkanBindingDataFactory(
      VulkanBackend* backend, int max_buffer_slots,
      std::unique_ptr<VulkanDescriptorPool> descriptor_pool,
      absl::Span<const Binding> bindings);
  bool AddBindingGroup();

  VulkanBackend* const backend_ = nullptr;
  const std::vector<Binding> bindings_;
  std::unique_ptr<VulkanDescriptorPool> descriptor_pool_;
  std::vector<BindingGroup> binding_groups_;
  int buffer_count_ = 0;
  int binding_size_ = 0;
  int buffer_value_alignment_ = 0;
  int buffer_slots_ = 0;
  int free_index_ = -1;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_BINDING_DATA_FACTORY_H_
