// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_MIPMAPS_H_
#define GB_RENDER_MIPMAPS_H_

#include <cstdint>

#include "gb/render/render_types.h"
#include "gb/render/sampler_options.h"

namespace gb {

//==============================================================================
// Mipmaps generated on the CPU, in a mip chain: a buffer holding each mip level
// in turn, starting with the texture itself (level 0), each tightly packed and
// half the width and height of the one before.
//==============================================================================

// Returns how many mip levels a texture of this size has with these sampler
// options, including the texture itself.
int GetMipLevelCount(int width, int height, const SamplerOptions& options);

// Returns the offset in bytes of a mip level in a mip chain.
int64_t GetMipLevelOffset(TextureFormat format, int width, int height,
                          int mip_level);

// Returns the size in bytes of a mip chain.
inline int64_t GetMipChainSizeInBytes(TextureFormat format, int width,
                                      int height, int mip_levels) {
  return GetMipLevelOffset(format, width, height, mip_levels);
}

// Generates mip levels 1 onward of the mip chain in "texels" from level 0.
//
// If "tile_size" is not zero, each tile of that size is resized on its own, so
// tiles don't blend into each other (the tile size halves with each level).
void GenerateMipmaps(TextureFormat format, int width, int height,
                     int mip_levels, int tile_size, void* texels);

}  // namespace gb

#endif  // GB_RENDER_MIPMAPS_H_
