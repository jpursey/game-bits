// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEXTURE_H_
#define GB_RENDER_TEXTURE_H_

#include "absl/types/span.h"
#include "gb/render/sampler_options.h"
#include "gb/render/texture_view.h"
#include "gb/resource/resource.h"

namespace gb {

// A texture defines a 2D image of RGBA pixels accessible by the graphics card.
//
// Textures are bound to shaders via binding data (see BindingData), and can
// also be changed or edited depending on its data volatility.
//
// This class and all derived classes must be thread-compatible.
class Texture : public Resource {
 public:
  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the data volatility for the vertices, specifying how often it is
  // likely to be edited and the relative memory overhead of doing so.
  DataVolatility GetVolatility() const { return volatility_; }

  // Returns the current width and height in pixels. This may be changed by
  // calling Set.
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

  // Returns the sampler options used with this texture.
  const SamplerOptions& GetSamplerOptions() const { return options_; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Clears the entire texture with a specific color (by default, transparent
  // black).
  bool Clear(Pixel pixel = {0, 0, 0, 0});
  bool Clear(uint32_t pixel);

  // Replaces the entire texture with the specified colors.
  //
  // The pixels array must contain at least GetWidth() * GetHeight() pixels.
  bool Set(absl::Span<const Pixel> pixels);
  bool Set(absl::Span<const uint32_t> pixels);
  bool Set(const void* pixels, size_t size_in_bytes);

  // Clears a region of the texture with the specified colors.
  //
  // If the destination region specified has any portion out of bounds relative
  // to the texture's dimensions, only the portion that intersects the image
  // will be cleared.
  //
  // Note: A kStaticWrite texture data may not be readable, so calling
  // ClearRegion may not be supported for only a subset of the image if it is
  // currently being (or was recently) rendered, in which case this will return
  // false.
  bool ClearRegion(int x, int y, int width, int height,
                   Pixel pixel = {0, 0, 0, 0});
  bool ClearRegion(int x, int y, int width, int height, uint32_t pixel);

  // Updates a region of the texture with the specified colors.
  //
  // If the destination region specified has any portion out of bounds relative
  // to the texture's dimensions, only the portion that intersects the image
  // will be copied. The pixels array must contain at least width * height
  // pixels.
  //
  // Note: A kStaticWrite texture data may not be readable, so calling SetRegion
  // may not be supported for only a subset of the image if it is currently
  // being (or was recently) rendered, in which case this will return false.
  bool SetRegion(int x, int y, int width, int height,
                 absl::Span<const Pixel> pixels);
  bool SetRegion(int x, int y, int width, int height,
                 absl::Span<const uint32_t> pixels);
  bool SetRegion(int x, int y, int width, int height, const void* pixels,
                 size_t size_in_bytes);

  // Returns an editable view onto the texture.
  //
  // This may be called for kPerFrame or kStaticReadWrite volatility textures
  // only, but may have additional overhead for kStaticReadWrite volatility. Any
  // changes to a view are propagated to the texture when the TextureView is
  // destructed, and will be visible the next time RenderSystem::EndFrame is
  // called.
  //
  // Modifying a texture via an EditView results in the entire texture getting
  // updated in the GPU. If only a portion or portions of a texture need to be
  // updated, prefer calling SetRegion instead. If no modifications are made
  // through an EditView, then this is not an issue.
  //
  // Only one TextureView may be active at any given time. If Edit is called
  // again before the a previous TextureView is destructed, this will return
  // null. The TextureView will also be null if the mesh volatility is
  // kStaticWrite.
  std::unique_ptr<TextureView> Edit();

 protected:
  Texture(ResourceEntry entry, DataVolatility volatility, int width, int height,
          const SamplerOptions& options)
      : Resource(std::move(entry)),
        volatility_(volatility),
        width_(width),
        height_(height),
        options_(options) {}
  virtual ~Texture() = default;

  // Clear the texture with the specified color, returning true if the write was
  // begun successfully.
  //
  // This will never be called if editing is process (DoEdit was called but
  // OnEditEnd was not).
  //
  // If this returns true, and the texture is readable (it isn't kStaticWrite
  // volatility), then it must reflect the data written. Returning true also
  // requires that the data will be transferred to the GPU before this texture
  // is used in rendering -- although it does not imply the transfer has
  // happened yet.
  virtual bool DoClear(int x, int y, int width, int height, Pixel pixel) = 0;

  // Write new data to the texture, returning true if the write was begun
  // successfully.
  //
  // This will never be called if editing is process (DoEdit was called but
  // OnEditEnd was not).
  //
  // If this returns true, and the texture is readable (it isn't kStaticWrite
  // volatility), then it must reflect the data written. Returning true also
  // requires that the data will be transferred to the GPU before this texture
  // is used in rendering -- although it does not imply the transfer has
  // happened yet.
  virtual bool DoSet(int x, int y, int width, int height, const void* pixels,
                     int stride) = 0;

  // Return an editable pointer to the texture.
  //
  // This will never be called for kStaticWrite volatility textures, or if
  // editing is already in process.
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
  friend class TextureView;

  // Returns true if the region is fully clipped, false width or height is
  // greater than zero after clipping.
  bool Clip(int* x, int* y, int* width, int* height,
            const void** pixels = nullptr);

  void OnViewDeleted(bool modified);

  const DataVolatility volatility_;
  const int width_;
  const int height_;
  const SamplerOptions options_;
  bool editing_ = false;
};

inline bool Texture::Clear(uint32_t pixel) { return Clear(Pixel(pixel)); }

inline bool Texture::Set(absl::Span<const Pixel> pixels) {
  return Set(pixels.data(), pixels.size() * sizeof(Pixel));
}

inline bool Texture::Set(absl::Span<const uint32_t> pixels) {
  return Set(pixels.data(), pixels.size() * sizeof(uint32_t));
}

inline bool Texture::ClearRegion(int x, int y, int width, int height,
                                 uint32_t pixel) {
  return ClearRegion(x, y, width, height, Pixel(pixel));
}

inline bool Texture::SetRegion(int x, int y, int width, int height,
                               absl::Span<const Pixel> pixels) {
  return SetRegion(x, y, width, height, pixels.data(),
                   pixels.size() * sizeof(Pixel));
}

inline bool Texture::SetRegion(int x, int y, int width, int height,
                               absl::Span<const uint32_t> pixels) {
  return SetRegion(x, y, width, height, pixels.data(),
                   pixels.size() * sizeof(uint32_t));
}

}  // namespace gb

#endif  // GB_RENDER_TEXTURE_H_
