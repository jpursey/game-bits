// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VOLUME_TEXTURE_H_
#define GB_RENDER_VOLUME_TEXTURE_H_

#include <cstdint>
#include <utility>

#include "gb/image/pixel.h"
#include "gb/render/render_types.h"
#include "gb/render/sampler_options.h"
#include "gb/resource/resource.h"

namespace gb {

// A volume texture defines a 3D image accessible by the graphics card, in any
// TextureFormat. It has no mipmaps.
//
// Volume textures are bound to shaders via binding data (see BindingData), as
// a sampler3D in GLSL, and can also be changed depending on their data
// volatility.
//
// Texels are taken and returned in the volume texture's format, ordered x
// first, then y, then z.
//
// This class and all derived classes must be thread-compatible.
class VolumeTexture : public Resource {
 public:
  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the data volatility for the texels, specifying how often they are
  // likely to be edited and the relative memory overhead of doing so.
  DataVolatility GetVolatility() const { return volatility_; }

  // Returns the width, height, and depth in texels.
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }
  int GetDepth() const { return depth_; }

  // Returns the format of each texel.
  TextureFormat GetFormat() const { return format_; }

  // Returns the sampler options used with this volume texture.
  const SamplerOptions& GetSamplerOptions() const { return options_; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Clears the whole volume with a specific color (by default, transparent
  // black), converted to the format (see PixelToTexel).
  bool Clear(Pixel pixel = {0, 0, 0, 0});

  // Replaces the whole volume with the texels, which must hold at least
  // GetWidth() * GetHeight() * GetDepth() texels.
  bool Set(const void* texels, int64_t size_in_bytes);

  // Reads the whole volume into "out_texels", which must hold at least
  // GetWidth() * GetHeight() * GetDepth() texels.
  //
  // This fails for volume textures with kStaticWrite volatility.
  bool Get(void* out_texels, int64_t size_in_bytes) const;

  // Clears a region of the volume with a specific color, converted to the
  // format.
  //
  // If the region has any portion out of bounds, only the portion that
  // intersects the volume is cleared.
  //
  // Note: A kStaticWrite volume texture's data may not be readable, so calling
  // ClearRegion may not be supported for only part of the volume if it is
  // currently being (or was recently) rendered, in which case this will return
  // false.
  bool ClearRegion(int x, int y, int z, int width, int height, int depth,
                   Pixel pixel = {0, 0, 0, 0});

  // Replaces a region of the volume with the texels, which must hold at least
  // width * height * depth texels.
  //
  // If the region has any portion out of bounds, only the portion that
  // intersects the volume is copied.
  //
  // Note: A kStaticWrite volume texture's data may not be readable, so calling
  // SetRegion may not be supported for only part of the volume if it is
  // currently being (or was recently) rendered, in which case this will return
  // false.
  bool SetRegion(int x, int y, int z, int width, int height, int depth,
                 const void* texels, int64_t size_in_bytes);

 protected:
  VolumeTexture(ResourceEntry entry, DataVolatility volatility, int width,
                int height, int depth, const SamplerOptions& options,
                TextureFormat format)
      : Resource(std::move(entry)),
        volatility_(volatility),
        width_(width),
        height_(height),
        depth_(depth),
        options_(options),
        format_(format) {}
  virtual ~VolumeTexture() = default;

  // Clears a region of the volume with the texel, returning true if the write
  // was begun successfully. The region is within the volume, and "texel"
  // points to GetTexelSize() bytes in the volume's format.
  //
  // If this returns true, and the volume is readable (it isn't kStaticWrite
  // volatility), then it must reflect the data written. Returning true also
  // requires that the data will be transferred to the GPU before this volume is
  // used in rendering -- although it does not imply the transfer has happened
  // yet.
  virtual bool DoClear(int x, int y, int z, int width, int height, int depth,
                       const void* texel) = 0;

  // Writes a region of the volume from the texels, returning true if the write
  // was begun successfully. The region is within the volume, and the texels are
  // in the volume's format, with rows "stride" texels apart, and slices
  // "slice_height" rows apart.
  //
  // If this returns true, and the volume is readable (it isn't kStaticWrite
  // volatility), then it must reflect the data written. Returning true also
  // requires that the data will be transferred to the GPU before this volume is
  // used in rendering -- although it does not imply the transfer has happened
  // yet.
  virtual bool DoSet(int x, int y, int z, int width, int height, int depth,
                     const void* texels, int stride, int slice_height) = 0;

  // Reads the whole volume into out_texels, which holds width * height * depth
  // texels, returning true if the read was completed successfully.
  //
  // This is never called for kStaticWrite volatility volume textures.
  virtual bool DoGet(void* out_texels) const = 0;

 private:
  // Clips a region to the volume, returning true if nothing remains. If
  // "texels" is not null, it holds the region's texels, and is moved to the
  // first texel that remains.
  bool Clip(int* x, int* y, int* z, int* width, int* height, int* depth,
            const void** texels = nullptr) const;

  const DataVolatility volatility_;
  const int width_;
  const int height_;
  const int depth_;
  const SamplerOptions options_;
  const TextureFormat format_;
};

}  // namespace gb

#endif  // GB_RENDER_VOLUME_TEXTURE_H_
