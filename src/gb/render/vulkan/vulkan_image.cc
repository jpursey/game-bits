// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_image.h"

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "gb/render/render_assert.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_format.h"

namespace gb {

namespace {

// Returns true if the device supports images like create_info describes,
// logging why not otherwise. The device's limits depend on the image's format,
// type, tiling, and usage. As the size of an image's memory isn't known until
// it is created, this sets max_resource_size to the limit on it.
bool ValidateImage(vk::PhysicalDevice physical_device,
                   const vk::ImageCreateInfo& create_info,
                   vk::DeviceSize* max_resource_size) {
  auto [result, properties] = physical_device.getImageFormatProperties(
      create_info.format, create_info.imageType, create_info.tiling,
      create_info.usage, create_info.flags);
  if (result != vk::Result::eSuccess) {
    LOG(ERROR) << "Images in format " << vk::to_string(create_info.format)
               << " with usage " << vk::to_string(create_info.usage)
               << " are not supported";
    return false;
  }
  const vk::Extent3D& extent = create_info.extent;
  const vk::Extent3D& max_extent = properties.maxExtent;
  if (extent.width > max_extent.width || extent.height > max_extent.height ||
      extent.depth > max_extent.depth) {
    LOG(ERROR) << "Image size " << extent.width << "," << extent.height << ","
               << extent.depth << " exceeds the device's limit of "
               << max_extent.width << "," << max_extent.height << ","
               << max_extent.depth;
    return false;
  }
  if (create_info.mipLevels > properties.maxMipLevels ||
      create_info.arrayLayers > properties.maxArrayLayers ||
      !(properties.sampleCounts & create_info.samples)) {
    LOG(ERROR) << "Image with " << create_info.mipLevels << " mip levels, "
               << create_info.arrayLayers << " layers, and "
               << vk::to_string(create_info.samples)
               << " samples exceeds the device's limits";
    return false;
  }
  *max_resource_size = properties.maxResourceSize;
  return true;
}

}  // namespace

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
  const bool is_3d = options.view_type == vk::ImageViewType::e3D;
  RENDER_ASSERT(is_3d ? layers_ == 1 : options.depth == 1);
  const vk::ImageType image_type =
      is_3d ? vk::ImageType::e3D : vk::ImageType::e2D;
  auto create_info = vk::ImageCreateInfo()
                         .setImageType(image_type)
                         .setExtent({static_cast<uint32_t>(width_),
                                     static_cast<uint32_t>(height_),
                                     static_cast<uint32_t>(options.depth)})
                         .setMipLevels(mip_levels_)
                         .setArrayLayers(layers_)
                         .setFormat(format_)
                         .setTiling(options.tiling)
                         .setInitialLayout(vk::ImageLayout::eUndefined)
                         .setUsage(usage)
                         .setSharingMode(vk::SharingMode::eExclusive)
                         .setSamples(options.sample_count)
                         .setFlags({});
  vk::DeviceSize max_resource_size = 0;
  if (!ValidateImage(backend_->GetPhysicalDevice(), create_info,
                     &max_resource_size)) {
    return false;
  }

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
  if (allocation_info.size > max_resource_size) {
    LOG(ERROR) << "Image size of " << allocation_info.size
               << " bytes exceeds the device's limit of " << max_resource_size;
    return false;
  }

  const vk::ImageAspectFlags aspect_flags =
      (IsDepthFormat(format_) ? vk::ImageAspectFlagBits::eDepth
                              : vk::ImageAspectFlagBits::eColor);
  auto [create_view_result, image_view] = device.createImageView(
      vk::ImageViewCreateInfo()
          .setImage(image_)
          .setViewType(options.view_type)
          .setFormat(format_)
          .setComponents(options.components)
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
