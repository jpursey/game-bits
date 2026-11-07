// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEXTURE_H_
#define GB_RENDER_TEXTURE_H_

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "absl/types/span.h"
#include "gb/image/image_view.h"
#include "gb/image/pixel.h"
#include "gb/render/render_types.h"
#include "gb/render/sampler_options.h"
#include "gb/resource/resource.h"

namespace gb {

// A texture defines a 2D image accessible by the graphics card, in any
// TextureFormat.
//
// Textures are bound to shaders via binding data (see BindingData), and can
// also be changed or edited depending on its data volatility.
//
// Functions that take texels as bytes take them in the texture's format. Those
// that take Pixel or uint32_t pixels, and Edit(), work only for formats whose
// texels are Pixels (see IsPixelFormat), and fail for the others.
//
// This class and all derived classes must be thread-compatible.
class Texture : public Resource {
 public:
  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the data volatility for the pixels, specifying how often they are
  // likely to be edited and the relative memory overhead of doing so.
  DataVolatility GetVolatility() const { return volatility_; }

  // Returns the width and height in texels.
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

  // Returns the format of each texel.
  TextureFormat GetFormat() const { return format_; }

  // Returns the sampler options used with this texture.
  const SamplerOptions& GetSamplerOptions() const { return options_; }

  // Returns true if this is a RenderTarget, which passes draw into.
  virtual bool IsRenderTarget() const { return false; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Clears the entire texture with a specific color (by default, transparent
  // black), converted to the texture's format (see PixelToTexel).
  bool Clear(Pixel pixel = {0, 0, 0, 0});
  bool Clear(uint32_t pixel);

  // Replaces the entire texture with the specified texels.
  //
  // The texels must contain at least GetWidth() * GetHeight() texels.
  bool Set(absl::Span<const Pixel> pixels);
  bool Set(absl::Span<const uint32_t> pixels);
  bool Set(const void* texels, int64_t size_in_bytes);

  // Reads the entire texture into "out_texels", which must hold at least
  // GetWidth() * GetHeight() texels.
  //
  // This fails for textures with kStaticWrite volatility, or while an
  // ImageView is active.
  bool Get(void* out_texels, int64_t size_in_bytes);

  // Clears a region of the texture with the specified colors.
  //
  // If the destination region specified has any portion out of bounds relative
  // to the texture's dimensions, only the portion that intersects the image
  // will be cleared.
  //
  // Note: A kStaticWrite texture data may not be readable, so calling
  // ClearRegion may not be supported for only a subset of the image if it is
  // currently being (or was recently) rendered, or is mipmapped, in which case
  // this will return false.
  bool ClearRegion(int x, int y, int width, int height,
                   Pixel pixel = {0, 0, 0, 0});
  bool ClearRegion(int x, int y, int width, int height, uint32_t pixel);

  // Updates a region of the texture with the specified texels.
  //
  // If the destination region specified has any portion out of bounds relative
  // to the texture's dimensions, only the portion that intersects the image
  // will be copied. The texels must contain at least width * height texels.
  //
  // Note: A kStaticWrite texture data may not be readable, so calling SetRegion
  // may not be supported for only a subset of the image if it is currently
  // being (or was recently) rendered, or is mipmapped, in which case this will
  // return false.
  bool SetRegion(int x, int y, int width, int height,
                 absl::Span<const Pixel> pixels);
  bool SetRegion(int x, int y, int width, int height,
                 absl::Span<const uint32_t> pixels);
  bool SetRegion(int x, int y, int width, int height, const void* texels,
                 int64_t size_in_bytes);

  // Returns an editable view onto the texture.
  //
  // This may be called for kPerFrame or kStaticReadWrite volatility textures
  // in a Pixel format only, but may have additional overhead for
  // kStaticReadWrite volatility. Any changes to a view are propagated to the
  // texture when the ImageView is destructed, and will be visible the next
  // time RenderSystem::EndFrame is called.
  //
  // Modifying a texture via an EditView results in the entire texture getting
  // updated in the GPU. If only a portion or portions of a texture need to be
  // updated, prefer calling SetRegion instead. If no modifications are made
  // through an EditView, then this is not an issue.
  //
  // Only one ImageView may be active at any given time. If Edit is called
  // again before a previous ImageView is destructed, this will return
  // null. The ImageView will also be null if the texture volatility is
  // kStaticWrite. The ImageView must be destroyed before the texture is
  // (deleting the texture first is a CHECK failure).
  std::unique_ptr<ImageView> Edit();

 protected:
  Texture(ResourceEntry entry, DataVolatility volatility, int width, int height,
          const SamplerOptions& options, TextureFormat format)
      : Resource(std::move(entry)),
        volatility_(volatility),
        width_(width),
        height_(height),
        options_(options),
        format_(format) {}
  virtual ~Texture();

  // Clear a region of the texture with the specified texel, returning true if
  // the write was begun successfully. "texel" points to GetTexelSize() bytes
  // in the texture's format.
  //
  // This will never be called if editing is in progress (DoEdit was called but
  // OnEditEnd was not).
  //
  // If this returns true, and the texture is readable (it isn't kStaticWrite
  // volatility), then it must reflect the data written. Returning true also
  // requires that the data will be transferred to the GPU before this texture
  // is used in rendering -- although it does not imply the transfer has
  // happened yet.
  virtual bool DoClear(int x, int y, int width, int height,
                       const void* texel) = 0;

  // Write new data to the texture, returning true if the write was begun
  // successfully. The texels are in the texture's format, and "stride" is the
  // number of texels from one row to the next.
  //
  // This will never be called if editing is in progress (DoEdit was called but
  // OnEditEnd was not).
  //
  // If this returns true, and the texture is readable (it isn't kStaticWrite
  // volatility), then it must reflect the data written. Returning true also
  // requires that the data will be transferred to the GPU before this texture
  // is used in rendering -- although it does not imply the transfer has
  // happened yet.
  virtual bool DoSet(int x, int y, int width, int height, const void* texels,
                     int stride) = 0;

  // Reads the whole texture into out_texels, returning true if the read was
  // completed successfully.
  //
  // "out_texels" holds width * height texels (so row stride is the width).
  // This is never called for kStaticWrite volatility textures, or if editing is
  // in progress.
  virtual bool DoGet(void* out_texels) = 0;

  // Return an editable pointer to the texture.
  //
  // This will never be called for kStaticWrite volatility textures, or if
  // editing is already in progress.
  //
  // This should return null on error. If this returns non-null, then the data
  // should be considered volatile and not be uploaded to the GPU until
  // OnEditEnd is called. OnEditEnd will only be called if this returns a
  // non-null value.
  virtual void* DoEditBegin() = 0;

  // Called to indicate editing has completed.
  //
  // The data should be transferred to the GPU before this texture is used in
  // rendering. If modified is false, then no editing took place, and no
  // transfer is needed.
  virtual void OnEditEnd(bool modified) = 0;

 private:
  // A render target's size changes as it is resized.
  friend class RenderTarget;
  void SetSize(int width, int height) {
    width_ = width;
    height_ = height;
  }

  // Returns true if the region is fully clipped, false if width and height are
  // greater than zero after clipping.
  bool Clip(int* x, int* y, int* width, int* height,
            const void** texels = nullptr);

  // Returns true if the texture's texels are Pixels, logging why "operation"
  // fails if not.
  bool ValidatePixelFormat(std::string_view operation) const;

  void OnViewDeleted(bool modified);

  const DataVolatility volatility_;
  int width_;
  int height_;
  const SamplerOptions options_;
  const TextureFormat format_;
  bool editing_ = false;
};

inline bool Texture::Clear(uint32_t pixel) { return Clear(Pixel(pixel)); }

inline bool Texture::Set(absl::Span<const Pixel> pixels) {
  if (!ValidatePixelFormat("Set")) {
    return false;
  }
  return Set(pixels.data(),
             static_cast<int64_t>(pixels.size() * sizeof(Pixel)));
}

inline bool Texture::Set(absl::Span<const uint32_t> pixels) {
  if (!ValidatePixelFormat("Set")) {
    return false;
  }
  return Set(pixels.data(),
             static_cast<int64_t>(pixels.size() * sizeof(uint32_t)));
}

inline bool Texture::ClearRegion(int x, int y, int width, int height,
                                 uint32_t pixel) {
  return ClearRegion(x, y, width, height, Pixel(pixel));
}

inline bool Texture::SetRegion(int x, int y, int width, int height,
                               absl::Span<const Pixel> pixels) {
  if (!ValidatePixelFormat("SetRegion")) {
    return false;
  }
  return SetRegion(x, y, width, height, pixels.data(),
                   static_cast<int64_t>(pixels.size() * sizeof(Pixel)));
}

inline bool Texture::SetRegion(int x, int y, int width, int height,
                               absl::Span<const uint32_t> pixels) {
  if (!ValidatePixelFormat("SetRegion")) {
    return false;
  }
  return SetRegion(x, y, width, height, pixels.data(),
                   static_cast<int64_t>(pixels.size() * sizeof(uint32_t)));
}

}  // namespace gb

#endif  // GB_RENDER_TEXTURE_H_
