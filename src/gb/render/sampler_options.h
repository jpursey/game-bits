// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_SAMPLER_OPTIONS_H_
#define GB_RENDER_SAMPLER_OPTIONS_H_

#include "gb/render/render_types.h"

namespace gb {

// Addressing mode when referencing texels outside the texture.
enum class SamplerAddressMode : uint8_t {
  kRepeat,        // Texture is repeated.
  kMirrorRepeat,  // Texture is mirrored and repeated.
  kClampEdge,     // Edge pixels are replicated.
  kClampBorder,   // Texture is clamped to border color.
};

// Texture sampler options specify how a texture will be applied in shaders.
//
// SamplerOptions may be a key in hash tables and can be compared for
// equality/inequality.
struct SamplerOptions {
  // Construction
  SamplerOptions() = default;

  // Enables texture filtering.
  SamplerOptions& SetFilter(bool in_filter) {
    filter = in_filter;
    return *this;
  }

  // Enables mipmaps for the texture.
  //
  // This is ignored for texture arrays, which are never mipmapped.
  SamplerOptions& SetMipmap(bool in_mipmap) {
    mipmap = in_mipmap;
    return *this;
  }

  // Addressing mode when referencing texels outside the texture.
  //
  // The border color is only relevant for kClampBorder address mode. Further,
  // it is only guaranteed to work for fully opaque or transparent pure black,
  // or fully opaque white. If arbitrary colors are not supported, but one is
  // specified, then the border will be opaque or transparent black (depending
  // on whether the color was mostly transparent or opaque).
  SamplerOptions& SetAddressMode(SamplerAddressMode in_address_mode,
                                 Pixel in_border = Pixel(0, 0, 0)) {
    address_mode = in_address_mode;
    border = in_border;
    return *this;
  }

  // If not zero, texture is treated as a square grid texture atlas with this
  // grid tile size. This affects mip map generation.
  //
  // This is ignored for texture arrays, which are never mipmapped.
  SamplerOptions& SetTileSize(int in_tile_size) {
    tile_size = in_tile_size;
    return *this;
  }

  // Data (see setters above for docs).
  bool filter = true;
  bool mipmap = true;
  SamplerAddressMode address_mode = SamplerAddressMode::kRepeat;
  Pixel border = Pixel(0, 0, 0);
  int tile_size = 0;
};

template <typename HashType>
inline HashType AbslHashValue(HashType hash, const SamplerOptions& options) {
  return HashType::combine(std::move(hash), options.filter, options.mipmap,
                           options.address_mode, options.border.Packed(),
                           options.tile_size);
}

inline bool operator==(const SamplerOptions& a, const SamplerOptions& b) {
  return a.filter == b.filter && a.mipmap == b.mipmap &&
         a.address_mode == b.address_mode && a.border == b.border &&
         a.tile_size == b.tile_size;
}

inline bool operator!=(const SamplerOptions& a, const SamplerOptions& b) {
  return !(a == b);
}

}  // namespace gb

#endif  // GB_RENDER_SAMPLER_OPTIONS_H_
