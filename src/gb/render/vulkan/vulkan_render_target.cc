// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_render_target.h"

#include <utility>

#include "gb/render/vulkan/vulkan_format.h"

namespace gb {

namespace {

std::unique_ptr<VulkanImage> CreateTargetImage(VulkanBackend* backend,
                                               vk::Format format, int width,
                                               int height) {
  const vk::ImageUsageFlags usage =
      vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
      (IsDepthFormat(format) ? vk::ImageUsageFlagBits::eDepthStencilAttachment
                             : vk::ImageUsageFlagBits::eColorAttachment);
  return VulkanImage::Create(backend, width, height, 1, format, usage,
                             VulkanImage::Options());
}

}  // namespace

VulkanRenderTarget* VulkanRenderTarget::Create(
    ResourceEntry entry, VulkanBackend* backend, vk::Sampler sampler,
    TextureFormat format, vk::Format vulkan_format,
    const RenderTargetSize& size, int width, int height,
    const SamplerOptions& options) {
  auto image = CreateTargetImage(backend, vulkan_format, width, height);
  if (image == nullptr) {
    return nullptr;
  }
  return new VulkanRenderTarget(std::move(entry), backend, sampler, format,
                                size, width, height, options, std::move(image));
}

VulkanRenderTarget::VulkanRenderTarget(
    ResourceEntry entry, VulkanBackend* backend, vk::Sampler sampler,
    TextureFormat format, const RenderTargetSize& size, int width, int height,
    const SamplerOptions& options, std::unique_ptr<VulkanImage> image)
    : RenderTarget(std::move(entry), format, size, width, height, options),
      backend_(backend),
      sampler_(sampler) {
  SetImage(std::move(image));
}

void VulkanRenderTarget::OnRender(VulkanRenderState* state) {
  if (!cleared_) {
    cleared_ = true;
    state->target_clears.push_back(image_.get());
  }
}

bool VulkanRenderTarget::DoResize(int width, int height) {
  auto image = CreateTargetImage(backend_, image_->GetFormat(), width, height);
  if (image == nullptr) {
    return false;
  }
  SetImage(std::move(image));
  return true;
}

void VulkanRenderTarget::SetImage(std::unique_ptr<VulkanImage> image) {
  // An image replaced is destroyed once no frame in flight uses it.
  image_ = std::move(image);
  image_handle_ = {image_handle_.version + 1, image_.get()};
  cleared_ = false;
}

}  // namespace gb
