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

// This interface defines the window requirements for Vulkan rendering.
//
// Derived class implementations must be thread-compatible.
class VulkanWindow {
 public:
  virtual ~VulkanWindow() = default;

  // Registers a callback which should be triggered when the window's render
  // size changes.
  virtual void SetSizeChangedCallback(
      gb::Callback<void()> size_changed_callback) {}

  // This returns any required extensions for rendering to the window.
  virtual bool GetExtensions(vk::Instance instance,
                             std::vector<const char*>* extensions) = 0;

  // This creates a new rendering surface for the window.
  virtual vk::SurfaceKHR CreateSurface(vk::Instance instance) = 0;

  // This returns the window size.
  virtual vk::Extent2D GetSize() = 0;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_WINDOW_H_
