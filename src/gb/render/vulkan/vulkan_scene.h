// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_SCENE_H_
#define GB_RENDER_VULKAN_VULKAN_SCENE_H_

#include "gb/render/render_scene.h"
#include "gb/render/vulkan/vulkan_scene_type.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

class VulkanScene : public RenderScene {
 public:
  static std::unique_ptr<VulkanScene> Create(VulkanInternal,
                                             VulkanSceneType* scene_type,
                                             int scene_order);

 private:
  VulkanScene(VulkanSceneType* scene_type, int scene_order,
              std::unique_ptr<VulkanBindingData> scene_data)
      : RenderScene(scene_type, scene_order, std::move(scene_data)) {}
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_SCENE_H_
