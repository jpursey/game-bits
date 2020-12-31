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

// Vulkan implementation of a texture.
//
// This class is thread-compatible, except as noted.
class VulkanTexture : public Texture {
 public:
  struct ImageHandle {
    int version;
    VulkanImage* image;
  };

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a new VulkanTexture.
  //
  // This is thread-safe.
  static VulkanTexture* Create(gb::ResourceEntry entry, VulkanBackend* backend,
                               vk::Sampler sampler, DataVolatility volatility,
                               int width, int height,
                               const SamplerOptions& options);

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
                int height, const SamplerOptions& options);
  ~VulkanTexture() override;

  struct DirtyRegion {
    DirtyRegion() = default;
    DirtyRegion(int x, int y, int width, int height)
        : x(x), y(y), width(width), height(height) {}

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
  };

  static void UnionDirtyRegion(DirtyRegion* region, int x, int y, int width,
                               int height) {
    const int x1 = std::min(x, region->x);
    const int y1 = std::min(y, region->y);
    const int x2 = std::max(x + width, region->x + region->width);
    const int y2 = std::max(y + height, region->y + region->height);
    *region = {x1, y1, x2 - x1, y2 - y1};
  }

  int GetMipLevels() const { return mip_levels_; }
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
  void UpdateImage(VulkanRenderState* state, VulkanBuffer* host_buffer,
                   VulkanImage* image, const DirtyRegion& region,
                   bool update_mips = true);

  // Derived class implementation.
  virtual bool Init() = 0;
  virtual void DoRender(VulkanRenderState* state) = 0;

 private:
  VulkanBackend* const backend_ = nullptr;
  const vk::Sampler sampler_;
  int mip_levels_ = 1;
  int host_size_ = 0;
  ImageHandle image_handles_[kMaxFramesInFlight] = {};
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_TEXTURE_H_
