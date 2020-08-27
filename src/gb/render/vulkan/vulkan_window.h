// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_WINDOW_H_
#define GB_RENDER_VULKAN_VULKAN_WINDOW_H_

#include <vector>

#include "gb/base/callback.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

class VulkanWindow {
 public:
  virtual ~VulkanWindow() = default;

  virtual void SetSizeChangedCallback(
      gb::Callback<void()> size_changed_callback) {}
  virtual bool GetExtensions(vk::Instance instance,
                             std::vector<const char*>* extensions) = 0;
  virtual vk::SurfaceKHR CreateSurface(vk::Instance instance) = 0;
  virtual vk::Extent2D GetSize() = 0;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_WINDOW_H_
