// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_VOLUME_TEXTURE_H_
#define GB_RENDER_VULKAN_VULKAN_VOLUME_TEXTURE_H_

#include <memory>

#include "gb/render/volume_texture.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_render_state.h"
#include "gb/render/vulkan/vulkan_texels.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Vulkan implementation of a volume texture.
//
// This class is thread-compatible, except as noted.
class VulkanVolumeTexture final : public VolumeTexture {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a new VulkanVolumeTexture, returning null if its images or host
  // buffers can't be created.
  //
  // This is thread-safe.
  static VulkanVolumeTexture* Create(
      gb::ResourceEntry entry, VulkanBackend* backend, vk::Sampler sampler,
      DataVolatility volatility, int width, int height, int depth,
      const SamplerOptions& options, TextureFormat format);

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
  bool DoClear(int x, int y, int z, int width, int height, int depth,
               const void* texel) override;
  bool DoSet(int x, int y, int z, int width, int height, int depth,
             const void* texels, int stride, int slice_height) override;
  bool DoGet(void* out_texels) const override;

 private:
  VulkanVolumeTexture(gb::ResourceEntry entry, vk::Sampler sampler,
                      DataVolatility volatility, int width, int height,
                      int depth, const SamplerOptions& options,
                      TextureFormat format,
                      std::unique_ptr<VulkanTexels> texels);
  ~VulkanVolumeTexture() override;

  const vk::Sampler sampler_;
  const std::unique_ptr<VulkanTexels> texels_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_VOLUME_TEXTURE_H_
