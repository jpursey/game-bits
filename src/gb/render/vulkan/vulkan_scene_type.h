// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_SCENE_TYPE_H_
#define GB_RENDER_VULKAN_VULKAN_SCENE_TYPE_H_

#include "gb/render/render_scene_type.h"
#include "gb/render/vulkan/vulkan_binding_data_factory.h"

namespace gb {

// Vulkan implementation of RenderSceneType.
//
// This class is thread-safe.
class VulkanSceneType final : public RenderSceneType {
 public:
  static std::unique_ptr<VulkanSceneType> Create(
      VulkanInternal, VulkanBackend* backend,
      absl::Span<const Binding> bindings);
  ~VulkanSceneType() override = default;

  vk::DescriptorSetLayout GetLayout() const {
    return scene_data_factory_->GetLayout();
  }
  std::unique_ptr<VulkanBindingData> CreateSceneBindingData() {
    return scene_data_factory_->NewBindingData(nullptr, BindingSet::kScene);
  }

 private:
  VulkanSceneType(absl::Span<const Binding> bindings,
                  std::unique_ptr<VulkanBindingDataFactory> scene_data_factory)
      : RenderSceneType(bindings),
        scene_data_factory_(std::move(scene_data_factory)) {}

  std::unique_ptr<VulkanBindingDataFactory> scene_data_factory_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_SCENE_TYPE_H_
