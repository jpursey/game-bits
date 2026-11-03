// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_volume_texture.h"

#include <cstdint>
#include <cstring>
#include <utility>

namespace gb {

TestVolumeTexture::TestVolumeTexture(Config* config, ResourceEntry entry,
                                     DataVolatility volatility, int width,
                                     int height, int depth,
                                     const SamplerOptions& options,
                                     TextureFormat format)
    : VolumeTexture(std::move(entry), volatility, width, height, depth, options,
                    format),
      config_(config),
      texels_(GetTextureSizeInBytes(format, width, height, depth), 0xFF) {}

bool TestVolumeTexture::IsInVolume(int x, int y, int z, int width, int height,
                                   int depth) const {
  return x >= 0 && y >= 0 && z >= 0 && width >= 0 && height >= 0 &&
         depth >= 0 && x + width <= GetWidth() && y + height <= GetHeight() &&
         z + depth <= GetDepth();
}

uint8_t* TestVolumeTexture::GetRow(int x, int y, int z) {
  const int64_t index =
      (int64_t{z} * GetHeight() + y) * GetWidth() + int64_t{x};
  return texels_.data() + index * GetTexelSize(GetFormat());
}

bool TestVolumeTexture::DoClear(int x, int y, int z, int width, int height,
                                int depth, const void* texel) {
  if (config_->fail_clear) {
    return false;
  }
  modify_count_ += 1;

  if (!IsInVolume(x, y, z, width, height, depth)) {
    invalid_call_count_ += 1;
    return false;
  }

  for (int slice = 0; slice < depth; ++slice) {
    for (int row = 0; row < height; ++row) {
      FillTexels(GetFormat(), texel, width, GetRow(x, y + row, z + slice));
    }
  }
  return true;
}

bool TestVolumeTexture::DoSet(int x, int y, int z, int width, int height,
                              int depth, const void* texels, int stride,
                              int slice_height) {
  if (config_->fail_set) {
    return false;
  }
  modify_count_ += 1;

  if (!IsInVolume(x, y, z, width, height, depth) || stride < width ||
      slice_height < height) {
    invalid_call_count_ += 1;
    return false;
  }

  const int texel_size = GetTexelSize(GetFormat());
  const uint8_t* const src_texels = static_cast<const uint8_t*>(texels);
  for (int slice = 0; slice < depth; ++slice) {
    for (int row = 0; row < height; ++row) {
      const int64_t src_index = (int64_t{slice} * slice_height + row) * stride;
      std::memcpy(GetRow(x, y + row, z + slice),
                  src_texels + src_index * texel_size,
                  int64_t{width} * texel_size);
    }
  }
  return true;
}

bool TestVolumeTexture::DoGet(void* out_texels) const {
  if (config_->fail_get) {
    return false;
  }
  std::memcpy(out_texels, texels_.data(), texels_.size());
  return true;
}

}  // namespace gb
