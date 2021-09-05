// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_IMAGE_H_
#define GB_RENDER_VULKAN_VULKAN_IMAGE_H_

#include <optional>

#include "gb/render/vulkan/vulkan_allocator.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Managed a Vulkan image resource, and its view / memory.
//
// This class is thread-safe.
class VulkanImage final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  struct Options {
    Options() = default;
    Options& SetMipLevels(int in_mip_levels) {
      mip_levels = in_mip_levels;
      return *this;
    }
    Options& SetTiling(vk::ImageTiling in_tiling) {
      tiling = in_tiling;
      return *this;
    }
    Options& SetSampleCount(vk::SampleCountFlagBits in_sample_count) {
      sample_count = in_sample_count;
      return *this;
    }
    Options& SetViewType(vk::ImageViewType in_view_type) {
      view_type = in_view_type;
      return *this;
    }

    int mip_levels = 1;
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1;
    vk::ImageViewType view_type = vk::ImageViewType::e2D;
  };

  static std::unique_ptr<VulkanImage> Create(VulkanBackend* backend, int width,
                                             int height, int layers,
                                             vk::Format format,
                                             vk::ImageUsageFlags usage,
                                             const Options& options);

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
  int GetLayers() const { return layers_; }
  int GetMipLevels() const { return mip_levels_; }

  vk::Image Get() const { return image_; }
  vk::ImageView GetView() const { return image_view_; }

 private:
  VulkanImage(VulkanBackend* backend, int width, int height, int layers,
              int mip_levels, vk::Format format)
      : backend_(backend),
        width_(width),
        height_(height),
        layers_(layers),
        mip_levels_(mip_levels),
        format_(format) {}

  bool Init(vk::ImageUsageFlags usage, const Options& options);

  VulkanBackend* const backend_;
  const int width_;
  const int height_;
  const int layers_;
  const int mip_levels_;
  const vk::Format format_;
  vk::Image image_;
  vk::ImageView image_view_ = {};
  VmaAllocation allocation_ = {};
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_IMAGE_H_
