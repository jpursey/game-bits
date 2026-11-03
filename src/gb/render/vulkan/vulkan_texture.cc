// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_texture.h"

#include <memory>

namespace gb {

VulkanTexture* VulkanTexture::Create(gb::ResourceEntry entry,
                                     VulkanBackend* backend,
                                     vk::Sampler sampler,
                                     DataVolatility volatility, int width,
                                     int height, const SamplerOptions& options,
                                     TextureFormat format) {
  auto texels = VulkanTexels::CreateTexture(backend, volatility, format, width,
                                            height, options);
  if (texels == nullptr) {
    return nullptr;
  }
  return new VulkanTexture(std::move(entry), sampler, volatility, width, height,
                           options, format, std::move(texels));
}

VulkanTexture::VulkanTexture(gb::ResourceEntry entry, vk::Sampler sampler,
                             DataVolatility volatility, int width, int height,
                             const SamplerOptions& options,
                             TextureFormat format,
                             std::unique_ptr<VulkanTexels> texels)
    : Texture(std::move(entry), volatility, width, height, options, format),
      sampler_(sampler),
      texels_(std::move(texels)) {}

VulkanTexture::~VulkanTexture() {}

bool VulkanTexture::DoClear(int x, int y, int width, int height,
                            const void* texel) {
  return texels_->Clear({x, y, 0, width, height, 1}, texel);
}

bool VulkanTexture::DoSet(int x, int y, int width, int height,
                          const void* texels, int stride) {
  return texels_->Set({x, y, 0, width, height, 1}, texels, stride, height);
}

bool VulkanTexture::DoGet(void* out_texels) { return texels_->Get(out_texels); }

void* VulkanTexture::DoEditBegin() { return texels_->EditBegin(); }

void VulkanTexture::OnEditEnd(bool modified) { texels_->EditEnd(modified); }

}  // namespace gb
