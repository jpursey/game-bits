// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_TEXELS_H_
#define GB_RENDER_VULKAN_VULKAN_TEXELS_H_

#include <cstdint>
#include <memory>

#include "gb/render/render_types.h"
#include "gb/render/sampler_options.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_buffer.h"
#include "gb/render/vulkan/vulkan_image.h"
#include "gb/render/vulkan/vulkan_render_state.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Holds the texels of a texture: its images, and the host buffers its data
// volatility needs. It queues the uploads of the regions written each frame,
// generating mipmaps when the texture has them.
//
// Regions are in three dimensions. Regions in a 2D texture have a z of zero
// and a depth of one.
//
// This class is thread-compatible.
class VulkanTexels {
 public:
  // The image to draw a frame with. The version changes whenever the image
  // does.
  struct ImageHandle {
    int version;
    VulkanImage* image;
  };

  // A region of texels, already clipped to the texture.
  struct Region {
    // Returns true if this region contains all of other.
    bool Contains(const Region& other) const {
      return other.x >= x && other.y >= y && other.z >= z &&
             other.x + other.width <= x + width &&
             other.y + other.height <= y + height &&
             other.z + other.depth <= z + depth;
    }

    int x = 0;
    int y = 0;
    int z = 0;
    int width = 0;
    int height = 0;
    int depth = 0;
  };

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates the texels of a 2D texture in the format, with mipmaps if options
  // asks for them.
  //
  // Returns null if the images or host buffers can't be created.
  static std::unique_ptr<VulkanTexels> CreateTexture(
      VulkanBackend* backend, DataVolatility volatility, TextureFormat format,
      int width, int height, const SamplerOptions& options);

  // Creates the texels of a 3D texture in the format, which has no mipmaps.
  //
  // Returns null if the images or host buffers can't be created.
  static std::unique_ptr<VulkanTexels> CreateVolume(VulkanBackend* backend,
                                                    DataVolatility volatility,
                                                    TextureFormat format,
                                                    int width, int height,
                                                    int depth);

  VulkanTexels(const VulkanTexels&) = delete;
  VulkanTexels& operator=(const VulkanTexels&) = delete;
  virtual ~VulkanTexels() = default;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  const ImageHandle& GetImageHandle(int frame) const {
    return image_handles_[frame % kMaxFramesInFlight];
  }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Sets every texel in a region to texel.
  bool Clear(const Region& region, const void* texel);

  // Writes a region from texels, whose rows are stride texels apart, and whose
  // slices are slice_height rows apart.
  bool Set(const Region& region, const void* texels, int stride,
           int slice_height);

  // Reads the whole texture, without mipmaps. Fails for kStaticWrite.
  bool Get(void* out_texels) const { return DoGet(out_texels); }

  // Returns the whole texture to change in place, or null if it can't be
  // (kStaticWrite). EditEnd() must follow, saying whether it changed.
  void* EditBegin() { return DoEditBegin(); }
  void EditEnd(bool modified) { DoEditEnd(modified); }

  //----------------------------------------------------------------------------
  // Events
  //----------------------------------------------------------------------------

  // Queues the uploads of the regions written since the frame's image was
  // last updated.
  void OnRender(VulkanRenderState* state) { DoRender(state); }

 protected:
  static constexpr int kNeverUsedFrame = -1000;

  // The format and size of a texture's texels, and the type of its images.
  struct Properties {
    TextureFormat format = TextureFormat::kRgba8Srgb;
    int width = 0;
    int height = 0;
    int depth = 1;
    vk::ImageViewType view_type = vk::ImageViewType::e2D;
    int mip_levels = 1;
    int tile_size = 0;
  };

  VulkanTexels(VulkanBackend* backend, const Properties& properties);

  //----------------------------------------------------------------------------
  // Shared by each data volatility
  //----------------------------------------------------------------------------

  int GetMipLevels() const { return mip_levels_; }
  Region GetWholeRegion() const { return {0, 0, 0, width_, height_, depth_}; }
  bool IsWholeRegion(const Region& region) const {
    return region.width == width_ && region.height == height_ &&
           region.depth == depth_;
  }
  bool FrameInUse(int frame) const { return backend_->IsFrameInUse(frame); }

  // Returns the size in bytes of the texels, without mipmaps.
  int64_t GetSizeInBytes() const {
    return GetTextureSizeInBytes(format_, width_, height_, depth_);
  }

  std::unique_ptr<VulkanBuffer> CreateHostBuffer();
  std::unique_ptr<VulkanImage> CreateImage();

  // Copies a region of the texels in src_texels, which are laid out as a host
  // buffer is, to buffer.
  void CopyHostRegion(void* buffer, const Region& region,
                      const void* src_texels) const;

  void SetAllImageHandles(VulkanImage* image);
  void SetImageHandle(int index, VulkanImage* image);

  // Queues the upload of a region from a host buffer to an image. The last
  // region uploaded to the image in a frame also generates and uploads its
  // mipmaps.
  void UpdateImage(VulkanRenderState* state, VulkanBuffer* host_buffer,
                   VulkanImage* image, const Region& region, bool last);

  //----------------------------------------------------------------------------
  // Derived class implementation
  //----------------------------------------------------------------------------

  virtual bool Init() = 0;

  // Prepares to write a region, marking it to upload, and returns the host
  // texels to write it to (laid out as a host buffer is), or null if it can't
  // be written.
  virtual void* PrepareWrite(const Region& region) = 0;

  virtual bool DoGet(void* out_texels) const = 0;
  virtual void* DoEditBegin() = 0;
  virtual void DoEditEnd(bool modified) = 0;
  virtual void DoRender(VulkanRenderState* state) = 0;

 private:
  // Creates the texels for the volatility's strategy.
  static std::unique_ptr<VulkanTexels> Create(VulkanBackend* backend,
                                              DataVolatility volatility,
                                              const Properties& properties);

  // Returns the offset in bytes of a region's first texel in a host buffer.
  int64_t GetRegionOffset(const Region& region) const;

  // Copies a region from src, whose rows are src_row_size bytes apart and
  // slices src_slice_size bytes apart, to a host buffer.
  void CopyRegion(void* buffer, const Region& region, const void* src,
                  int64_t src_row_size, int64_t src_slice_size) const;

  VulkanBackend* const backend_;
  const TextureFormat format_;
  const int width_;
  const int height_;
  const int depth_;
  const vk::ImageViewType view_type_;
  const int mip_levels_;
  const int tile_size_;
  const int64_t host_size_;
  ImageHandle image_handles_[kMaxFramesInFlight] = {};
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_TEXELS_H_
