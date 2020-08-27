// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_scene.h"

#include "absl/memory/memory.h"

namespace gb {

std::unique_ptr<VulkanScene> VulkanScene::Create(VulkanInternal,
                                                 VulkanSceneType* scene_type,
                                                 int scene_order) {
  auto scene_data = scene_type->CreateSceneBindingData();
  if (scene_data == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(
      new VulkanScene(scene_type, scene_order, std::move(scene_data)));
}

}  // namespace gb
