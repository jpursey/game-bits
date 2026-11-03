// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_volume_texture.h"

#include <memory>
#include <utility>

namespace gb {

VulkanVolumeTexture* VulkanVolumeTexture::Create(
    gb::ResourceEntry entry, VulkanBackend* backend, vk::Sampler sampler,
    DataVolatility volatility, int width, int height, int depth,
    const SamplerOptions& options, TextureFormat format) {
  auto texels = VulkanTexels::CreateVolume(backend, volatility, format, width,
                                           height, depth);
  if (texels == nullptr) {
    return nullptr;
  }
  return new VulkanVolumeTexture(std::move(entry), sampler, volatility, width,
                                 height, depth, options, format,
                                 std::move(texels));
}

VulkanVolumeTexture::VulkanVolumeTexture(
    gb::ResourceEntry entry, vk::Sampler sampler, DataVolatility volatility,
    int width, int height, int depth, const SamplerOptions& options,
    TextureFormat format, std::unique_ptr<VulkanTexels> texels)
    : VolumeTexture(std::move(entry), volatility, width, height, depth, options,
                    format),
      sampler_(sampler),
      texels_(std::move(texels)) {}

VulkanVolumeTexture::~VulkanVolumeTexture() {}

bool VulkanVolumeTexture::DoClear(int x, int y, int z, int width, int height,
                                  int depth, const void* texel) {
  return texels_->Clear({x, y, z, width, height, depth}, texel);
}

bool VulkanVolumeTexture::DoSet(int x, int y, int z, int width, int height,
                                int depth, const void* texels, int stride,
                                int slice_height) {
  return texels_->Set({x, y, z, width, height, depth}, texels, stride,
                      slice_height);
}

bool VulkanVolumeTexture::DoGet(void* out_texels) const {
  return texels_->Get(out_texels);
}

}  // namespace gb
