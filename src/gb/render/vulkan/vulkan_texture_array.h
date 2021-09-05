// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_TEXTURE_ARRAY_H_
#define GB_RENDER_VULKAN_VULKAN_TEXTURE_ARRAY_H_

#include <cstdint>
#include <tuple>

#include "absl/container/flat_hash_set.h"
#include "gb/file/file_types.h"
#include "gb/render/texture_array.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_buffer.h"
#include "gb/render/vulkan/vulkan_image.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Vulkan implementation of a texture.
//
// This class is thread-compatible, except as noted.
class VulkanTextureArray : public TextureArray {
 public:
  struct ImageHandle {
    int version;
    VulkanImage* image;
  };

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a new VulkanTextureArray.
  //
  // This is thread-safe.
  static VulkanTextureArray* Create(gb::ResourceEntry entry,
                                    VulkanBackend* backend, vk::Sampler sampler,
                                    DataVolatility volatility, int count,
                                    int width, int height,
                                    const SamplerOptions& options);

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  const ImageHandle& GetImageHandle() const { return image_handle_; }
  vk::Sampler GetSampler() const { return sampler_; }

  //----------------------------------------------------------------------------
  // Events
  //----------------------------------------------------------------------------

  void OnRender(VulkanRenderState* state);

 protected:
  VulkanTextureArray(gb::ResourceEntry entry, VulkanBackend* backend,
                     vk::Sampler sampler, DataVolatility volatility, int count,
                     int width, int height, const SamplerOptions& options);
  ~VulkanTextureArray() override;

  bool FrameInUse(int frame) const { return frame > backend_->GetFrame() - 2; }
  bool CreateImage();
  bool CreateHostBuffer();
  void UpdateImage(VulkanRenderState* state, int index);
  void* ModifyHostData(int index);

  // TextureArray implementation.
  bool DoClear(int index, Pixel pixel) override;
  bool DoSet(int index, const void* pixels) override;
  bool DoGet(int index, void* out_pixels) override;

 private:
  static constexpr int kNeverUsedFrame = -1000;

  VulkanBackend* const backend_ = nullptr;
  const vk::Sampler sampler_;
  int mip_levels_ = 1;
  int host_layer_size_ = 0;

  ImageHandle image_handle_ = {};
  int render_frame_ = kNeverUsedFrame;
  std::unique_ptr<VulkanImage> image_;
  std::unique_ptr<VulkanBuffer> host_buffer_;
  absl::flat_hash_set<int> updates_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_TEXTURE_ARRAY_H_
