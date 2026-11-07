// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_BINDING_LAYOUT_H_
#define GB_RENDER_VULKAN_VULKAN_BINDING_LAYOUT_H_

#include "gb/render/binding_layout.h"
#include "gb/render/vulkan/vulkan_binding_data_factory.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Vulkan implementation of BindingLayout.
//
// This class is thread-safe.
class VulkanBindingLayout final : public BindingLayout {
 public:
  static std::unique_ptr<VulkanBindingLayout> Create(
      VulkanInternal, VulkanBackend* backend,
      absl::Span<const Binding> bindings);
  ~VulkanBindingLayout() override = default;

  // Returns the descriptor set layout of the view binding set. The material
  // and instance sets' layouts belong to each VulkanRenderPipeline.
  vk::DescriptorSetLayout GetViewSetLayout() const {
    return view_data_factory_->GetLayout();
  }
  std::unique_ptr<VulkanBindingData> CreateViewBindingData() {
    return view_data_factory_->NewBindingData(nullptr, BindingSet::kView);
  }

 private:
  VulkanBindingLayout(
      absl::Span<const Binding> bindings,
      std::unique_ptr<VulkanBindingDataFactory> view_data_factory)
      : BindingLayout(bindings),
        view_data_factory_(std::move(view_data_factory)) {}

  std::unique_ptr<VulkanBindingDataFactory> view_data_factory_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_BINDING_LAYOUT_H_
