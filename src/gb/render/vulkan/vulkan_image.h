// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_IMAGE_H_
#define GB_RENDER_VULKAN_VULKAN_IMAGE_H_

#include "gb/render/vulkan/vulkan_allocator.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

class VulkanImage final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  static std::unique_ptr<VulkanImage> Create(
      VulkanBackend* backend, int width, int height, vk::Format format,
      vk::ImageUsageFlags usage,
      vk::ImageTiling tiling = vk::ImageTiling::eOptimal);
  VulkanImage(const VulkanImage&) = delete;
  VulkanImage(VulkanImage&&) = delete;
  VulkanImage& operator=(const VulkanImage&) = delete;
  VulkanImage& operator=(VulkanImage&&) = delete;
  ~VulkanImage();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

  vk::Image Get() const { return image_; }
  vk::ImageView GetView() const { return image_view_; }

 private:
  VulkanImage(VulkanBackend* backend, int width, int height, vk::Format format)
      : backend_(backend), width_(width), height_(height), format_(format) {}

  bool Init(vk::ImageUsageFlags usage, vk::ImageTiling tiling);

  VulkanBackend* const backend_;
  const int width_;
  const int height_;
  const vk::Format format_;
  vk::Image image_;
  vk::ImageView image_view_;
  VmaAllocation allocation_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_IMAGE_H_
