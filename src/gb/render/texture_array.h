// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEXTURE_ARRAY_H_
#define GB_RENDER_TEXTURE_ARRAY_H_

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gb/image/pixel.h"
#include "gb/render/render_types.h"
#include "gb/render/sampler_options.h"
#include "gb/resource/resource.h"

namespace gb {

// A texture array defines an array of 2D images, all the same size and in the
// same TextureFormat, accessible by the graphics card.
//
// Texture arrays are bound to shaders via binding data (see BindingData), and
// can also be changed depending on their data volatility.
//
// Functions that take texels as bytes take them in the texture array's format.
// Those that take Pixel or uint32_t pixels work only for formats whose texels
// are Pixels (see IsPixelFormat), and fail for the others.
//
// This class and all derived classes must be thread-compatible.
class TextureArray : public Resource {
 public:
  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the data volatility for the pixels, specifying how often they are
  // likely to be edited and the relative memory overhead of doing so.
  DataVolatility GetVolatility() const { return volatility_; }

  // Returns the number of textures in the array.
  int GetCount() const { return count_; }

  // Returns the width and height in texels.
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

  // Returns the format of each texel.
  TextureFormat GetFormat() const { return format_; }

  // Returns the sampler options used with this texture array.
  const SamplerOptions& GetSamplerOptions() const { return options_; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Clears a texture in the array with a specific color (by default,
  // transparent black), converted to the format (see PixelToTexel).
  bool Clear(int index, Pixel pixel = {0, 0, 0, 0});
  bool Clear(int index, uint32_t pixel);

  // Replaces a texture in the array with the specified texels.
  //
  // The texels must contain at least GetWidth() * GetHeight() texels.
  //
  // Note: A kStaticWrite texture array's data may not be readable, so calling
  // Clear or Set may not be supported for an array of more than one texture if
  // it is currently being (or was recently) rendered, in which case this will
  // return false.
  bool Set(int index, absl::Span<const Pixel> pixels);
  bool Set(int index, absl::Span<const uint32_t> pixels);
  bool Set(int index, const void* texels, int64_t size_in_bytes);

  // Reads the entirety of a texture into a provided buffer.
  //
  // This will always fail for texture arrays with kStaticWrite volatility. A
  // vector passed in is left unchanged for an invalid index or volatility.
  bool Get(int index, std::vector<Pixel>* out_pixels);
  bool Get(int index, std::vector<uint32_t>* out_pixels);
  bool Get(int index, void* out_texels, int64_t size_in_bytes);

 protected:
  TextureArray(ResourceEntry entry, DataVolatility volatility, int count,
               int width, int height, const SamplerOptions& options,
               TextureFormat format)
      : Resource(std::move(entry)),
        volatility_(volatility),
        count_(count),
        width_(width),
        height_(height),
        options_(options),
        format_(format) {}
  virtual ~TextureArray() = default;

  // Clear the texture at the specified index with the specified texel,
  // returning true if the write was begun successfully. "texel" points to
  // GetTexelSize() bytes in the texture array's format.
  //
  // If this returns true, and the texture array is readable (it isn't
  // kStaticWrite volatility), then it must reflect the data written if queried
  // via DoGet(). Returning true also requires that the data will be transferred
  // to the GPU before this texture is used in rendering -- although it does not
  // imply the transfer has happened yet.
  virtual bool DoClear(int index, const void* texel) = 0;

  // Write new data to the texture at the specified index, returning true if the
  // write was begun successfully.
  //
  // If this returns true, and the texture array is readable (it isn't
  // kStaticWrite volatility), then it must reflect the data written if queried
  // via DoGet(). Returning true also requires that the data will be transferred
  // to the GPU before this texture is used in rendering -- although it does not
  // imply the transfer has happened yet.
  virtual bool DoSet(int index, const void* texels) = 0;

  // Reads data from the texture at the specified index into out_texels,
  // returning true if the read was completed successfully.
  //
  // "out_texels" is preallocated to be width*height texels (so row stride is
  // the width). This is never called if the texture array is kStaticWrite
  // volatility.
  virtual bool DoGet(int index, void* out_texels) = 0;

 private:
  // Returns true if the texture at the index can be read, logging why not if it
  // can't.
  bool CanGet(int index) const;

  // Returns true if the texture array's texels are Pixels, logging why
  // "operation" fails if not.
  bool ValidatePixelFormat(std::string_view operation) const;

  const DataVolatility volatility_;
  const int count_;
  const int width_;
  const int height_;
  const SamplerOptions options_;
  const TextureFormat format_;
};

inline bool TextureArray::Clear(int index, uint32_t pixel) {
  return Clear(index, Pixel(pixel));
}

inline bool TextureArray::Set(int index, absl::Span<const Pixel> pixels) {
  if (!ValidatePixelFormat("Set")) {
    return false;
  }
  return Set(index, pixels.data(),
             static_cast<int64_t>(pixels.size() * sizeof(Pixel)));
}

inline bool TextureArray::Set(int index, absl::Span<const uint32_t> pixels) {
  if (!ValidatePixelFormat("Set")) {
    return false;
  }
  return Set(index, pixels.data(),
             static_cast<int64_t>(pixels.size() * sizeof(uint32_t)));
}

inline bool TextureArray::Get(int index, std::vector<Pixel>* out_pixels) {
  if (!ValidatePixelFormat("Get") || !CanGet(index)) {
    return false;
  }
  out_pixels->resize(width_ * height_);
  return DoGet(index, out_pixels->data());
}

inline bool TextureArray::Get(int index, std::vector<uint32_t>* out_pixels) {
  if (!ValidatePixelFormat("Get") || !CanGet(index)) {
    return false;
  }
  out_pixels->resize(width_ * height_);
  return DoGet(index, out_pixels->data());
}

}  // namespace gb

#endif  // GB_RENDER_TEXTURE_ARRAY_H_
