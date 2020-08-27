// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_TEXTURE_H_
#define GB_RENDER_VULKAN_VULKAN_TEXTURE_H_

#include <cstdint>
#include <tuple>

#include "gb/file/file_types.h"
#include "gb/render/texture.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_image.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

class VulkanTexture : public Texture {
 public:
  struct ImageHandle {
    int version;
    VulkanImage* image;
  };

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  static VulkanTexture* Create(gb::ResourceEntry entry, VulkanBackend* backend,
                               vk::Sampler sampler, DataVolatility volatility,
                               int width, int height);

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  const ImageHandle& GetImageHandle(int frame) const {
    return image_handles_[frame % kMaxFramesInFlight];
  }
  vk::Sampler GetSampler() const { return sampler_; }

  //----------------------------------------------------------------------------
  // Events
  //----------------------------------------------------------------------------

  void OnRender(VulkanRenderState* state) { DoRender(state); }

 protected:
  VulkanTexture(gb::ResourceEntry entry, VulkanBackend* backend,
                vk::Sampler sampler, DataVolatility volatility, int width,
                int height)
      : Texture(std::move(entry), volatility, width, height),
        backend_(backend),
        sampler_(sampler) {}
  ~VulkanTexture() override;

  bool FrameInUse(int frame) const { return frame > backend_->GetFrame() - 2; }
  std::unique_ptr<VulkanBuffer> CreateHostBuffer();
  std::unique_ptr<VulkanImage> CreateImage();
  void ClearRegion(void* buffer, int x, int y, int width, int height,
                   Pixel pixel);
  void CopyRegion(void* buffer, int x, int y, int width, int height,
                  const void* pixels, int stride);
  void SetAllImageHandles(VulkanImage* image) {
    for (ImageHandle& handle : image_handles_) {
      handle.version += 1;
      handle.image = image;
    }
  }
  void SetImageHandle(int index, VulkanImage* image) {
    auto& handle = image_handles_[index];
    handle.version += 1;
    handle.image = image;
  }

  // Derived class implementation.
  virtual bool Init() = 0;
  virtual void DoRender(VulkanRenderState* state) = 0;

 private:
  bool Init(vk::Sampler sampler, void* rgba);
  bool TransitionImageLayout(vk::ImageLayout old_layout,
                             vk::ImageLayout new_layout);

  VulkanBackend* const backend_ = nullptr;
  const vk::Sampler sampler_;
  ImageHandle image_handles_[kMaxFramesInFlight] = {};
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_TEXTURE_H_
