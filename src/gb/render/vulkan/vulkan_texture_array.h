// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_TEXTURE_ARRAY_H_
#define GB_RENDER_VULKAN_VULKAN_TEXTURE_ARRAY_H_

#include <cstdint>
#include <memory>

#include "absl/container/flat_hash_set.h"
#include "gb/render/texture_array.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_buffer.h"
#include "gb/render/vulkan/vulkan_image.h"
#include "gb/render/vulkan/vulkan_render_state.h"
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
                                    const SamplerOptions& options,
                                    TextureFormat format);

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
                     int width, int height, const SamplerOptions& options,
                     TextureFormat format);
  ~VulkanTextureArray() override;

  bool FrameInUse(int frame) const { return backend_->IsFrameInUse(frame); }
  bool CreateImage();
  std::unique_ptr<VulkanBuffer> CreateHostBuffer();
  bool EnsureHostBufferIsWritable();
  void UploadTexture(VulkanRenderState* state, int index);

  // Returns the size in bytes of one texture, without its mipmaps.
  int64_t GetLayerSizeInBytes() const {
    return GetTextureSizeInBytes(GetFormat(), GetWidth(), GetHeight());
  }

  // Returns the host data of a texture, followed by its mipmaps.
  uint8_t* GetHostData(int index);

  // Returns the host data of a texture to modify, or null if it can't be.
  uint8_t* ModifyHostData(int index);

  // TextureArray implementation.
  bool DoClear(int index, const void* texel) override;
  bool DoSet(int index, const void* texels) override;
  bool DoGet(int index, void* out_texels) override;

 private:
  static constexpr int kNeverUsedFrame = -1000;

  VulkanBackend* const backend_ = nullptr;
  const vk::Sampler sampler_;
  const int mip_levels_;
  const int64_t host_layer_size_;

  ImageHandle image_handle_ = {};
  int render_frame_ = kNeverUsedFrame;
  int transfer_frame_ = kNeverUsedFrame;
  std::unique_ptr<VulkanImage> image_;
  std::unique_ptr<VulkanBuffer> host_buffer_;

  // Textures modified since the last upload.
  absl::flat_hash_set<int> updates_;

  // Set when the image was replaced, so every texture must be uploaded.
  bool upload_all_ = false;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_TEXTURE_ARRAY_H_
