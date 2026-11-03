// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/mipmaps.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "gb/image/pixel.h"
#include "stb_image_resize.h"

namespace gb {

namespace {

// Resizes texels in a format whose channels are each one byte (any but kRgba4),
// so its texel size is its channel count.
void ResizeTexels(TextureFormat format, const uint8_t* src, int src_width,
                  int src_height, int src_stride, uint8_t* dst, int dst_width,
                  int dst_height, int dst_stride) {
  const int channels = GetTexelSize(format);
  stbir_resize_uint8_generic(
      src, src_width, src_height, src_stride, dst, dst_width, dst_height,
      dst_stride, channels, channels == 4 ? 3 : STBIR_ALPHA_CHANNEL_NONE, 0,
      STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT,
      format == TextureFormat::kRgba8Srgb ? STBIR_COLORSPACE_SRGB
                                          : STBIR_COLORSPACE_LINEAR,
      nullptr);
}

Pixel UnpackRgba4(const uint8_t* texel) {
  uint16_t value = 0;
  std::memcpy(&value, texel, sizeof(value));
  return Pixel(static_cast<uint8_t>(((value >> 12) & 0xF) * 17),
               static_cast<uint8_t>(((value >> 8) & 0xF) * 17),
               static_cast<uint8_t>(((value >> 4) & 0xF) * 17),
               static_cast<uint8_t>((value & 0xF) * 17));
}

// stb_image_resize has no 4-bit channels, so the mipmaps are generated as
// kRgba8, and packed.
void GenerateRgba4Mipmaps(int width, int height, int mip_levels, int tile_size,
                          uint8_t* texels) {
  constexpr int kTexelSize = GetTexelSize(TextureFormat::kRgba4);
  const int64_t texel_count =
      GetMipChainSizeInBytes(TextureFormat::kRgba4, width, height, mip_levels) /
      kTexelSize;
  const int64_t base_count = static_cast<int64_t>(width) * height;
  std::vector<Pixel> pixels(texel_count);
  for (int64_t i = 0; i < base_count; ++i) {
    pixels[i] = UnpackRgba4(texels + i * kTexelSize);
  }
  GenerateMipmaps(TextureFormat::kRgba8, width, height, mip_levels, tile_size,
                  pixels.data());
  for (int64_t i = base_count; i < texel_count; ++i) {
    const uint32_t texel = PixelToTexel(TextureFormat::kRgba4, pixels[i]);
    std::memcpy(texels + i * kTexelSize, &texel, kTexelSize);
  }
}

}  // namespace

int GetMipLevelCount(int width, int height, const SamplerOptions& options) {
  if (!options.mipmap) {
    return 1;
  }
  int size = options.tile_size;
  if (size == 0) {
    size = std::min(width, height);
  }
  int mip_levels = 1;
  for (size >>= 1; size != 0; size >>= 1) {
    ++mip_levels;
  }
  return mip_levels;
}

int64_t GetMipLevelOffset(TextureFormat format, int width, int height,
                          int mip_level) {
  int64_t offset = 0;
  for (int mip = 0; mip < mip_level; ++mip) {
    offset += GetTextureSizeInBytes(format, width >> mip, height >> mip);
  }
  return offset;
}

void GenerateMipmaps(TextureFormat format, int width, int height,
                     int mip_levels, int tile_size, void* texels) {
  if (mip_levels <= 1) {
    return;
  }
  if (format == TextureFormat::kRgba4) {
    GenerateRgba4Mipmaps(width, height, mip_levels, tile_size,
                         static_cast<uint8_t*>(texels));
    return;
  }

  const int texel_size = GetTexelSize(format);
  uint8_t* src = static_cast<uint8_t*>(texels);
  for (int mip = 1; mip < mip_levels; ++mip) {
    uint8_t* const dst = src + GetTextureSizeInBytes(format, width, height);
    const int dst_width = width >> 1;
    const int dst_height = height >> 1;
    const int src_stride = width * texel_size;
    const int dst_stride = dst_width * texel_size;
    if (tile_size == 0) {
      ResizeTexels(format, src, width, height, src_stride, dst, dst_width,
                   dst_height, dst_stride);
    } else {
      const int dst_tile_size = tile_size >> 1;
      for (int y = 0; y < height; y += tile_size) {
        for (int x = 0; x < width; x += tile_size) {
          ResizeTexels(format, src + y * src_stride + x * texel_size, tile_size,
                       tile_size, src_stride,
                       dst + (y / 2) * dst_stride + (x / 2) * texel_size,
                       dst_tile_size, dst_tile_size, dst_stride);
        }
      }
      tile_size = dst_tile_size;
    }
    src = dst;
    width = dst_width;
    height = dst_height;
  }
}

}  // namespace gb
