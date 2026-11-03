// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_TEXTURE_H_
#define GB_RENDER_VULKAN_VULKAN_TEXTURE_H_

#include <memory>

#include "gb/render/texture.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_render_state.h"
#include "gb/render/vulkan/vulkan_texels.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Vulkan implementation of a texture.
//
// This class is thread-compatible, except as noted.
class VulkanTexture final : public Texture {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a new VulkanTexture.
  //
  // This is thread-safe.
  static VulkanTexture* Create(gb::ResourceEntry entry, VulkanBackend* backend,
                               vk::Sampler sampler, DataVolatility volatility,
                               int width, int height,
                               const SamplerOptions& options,
                               TextureFormat format);

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  const VulkanTexels::ImageHandle& GetImageHandle(int frame) const {
    return texels_->GetImageHandle(frame);
  }
  vk::Sampler GetSampler() const { return sampler_; }

  //----------------------------------------------------------------------------
  // Events
  //----------------------------------------------------------------------------

  void OnRender(VulkanRenderState* state) { texels_->OnRender(state); }

 protected:
  bool DoClear(int x, int y, int width, int height, const void* texel) override;
  bool DoSet(int x, int y, int width, int height, const void* texels,
             int stride) override;
  bool DoGet(void* out_texels) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;

 private:
  VulkanTexture(gb::ResourceEntry entry, vk::Sampler sampler,
                DataVolatility volatility, int width, int height,
                const SamplerOptions& options, TextureFormat format,
                std::unique_ptr<VulkanTexels> texels);
  ~VulkanTexture() override;

  const vk::Sampler sampler_;
  const std::unique_ptr<VulkanTexels> texels_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_TEXTURE_H_
