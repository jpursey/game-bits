// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_image.h"

#include "absl/memory/memory.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_format.h"

namespace gb {

std::unique_ptr<VulkanImage> VulkanImage::Create(VulkanBackend* backend,
                                                 int width, int height,
                                                 int layers, vk::Format format,
                                                 vk::ImageUsageFlags usage,
                                                 const Options& options) {
  auto image = absl::WrapUnique(new VulkanImage(backend, width, height, layers,
                                                options.mip_levels, format));
  if (!image->Init(usage, options)) {
    return nullptr;
  }
  return image;
}

VulkanImage::~VulkanImage() {
  auto* gc = backend_->GetGarbageCollector();
  gc->Dispose(image_view_);
  gc->Dispose(image_, allocation_);
}

bool VulkanImage::Init(vk::ImageUsageFlags usage, const Options& options) {
  auto device = backend_->GetDevice();
  auto create_info = vk::ImageCreateInfo()
                         .setImageType(vk::ImageType::e2D)
                         .setExtent({static_cast<uint32_t>(width_),
                                     static_cast<uint32_t>(height_), 1})
                         .setMipLevels(mip_levels_)
                         .setArrayLayers(layers_)
                         .setFormat(format_)
                         .setTiling(options.tiling)
                         .setInitialLayout(vk::ImageLayout::eUndefined)
                         .setUsage(usage)
                         .setSharingMode(vk::SharingMode::eExclusive)
                         .setSamples(options.sample_count)
                         .setFlags({});

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  VkImage image = VK_NULL_HANDLE;
  VmaAllocationInfo allocation_info = {};
  if (vmaCreateImage(backend_->GetAllocator(), &(VkImageCreateInfo&)create_info,
                     &alloc_info, &image, &allocation_,
                     &allocation_info) != VK_SUCCESS) {
    return false;
  }
  image_ = image;

  const vk::ImageAspectFlags aspect_flags =
      (IsDepthFormat(format_) ? vk::ImageAspectFlagBits::eDepth
                              : vk::ImageAspectFlagBits::eColor);
  auto [create_view_result, image_view] = device.createImageView(
      vk::ImageViewCreateInfo()
          .setImage(image_)
          .setViewType(options.view_type)
          .setFormat(format_)
          .setSubresourceRange(vk::ImageSubresourceRange()
                                   .setAspectMask(aspect_flags)
                                   .setBaseMipLevel(0)
                                   .setLevelCount(mip_levels_)
                                   .setBaseArrayLayer(0)
                                   .setLayerCount(layers_)));
  if (create_view_result != vk::Result::eSuccess) {
    return false;
  }
  image_view_ = image_view;
  return true;
}

}  // namespace gb
