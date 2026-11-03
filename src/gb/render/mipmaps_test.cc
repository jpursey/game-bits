// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/mipmaps.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "gb/image/pixel.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

constexpr TextureFormat kAllFormats[] = {
    TextureFormat::kRgba8Srgb, TextureFormat::kRgba8, TextureFormat::kRg8,
    TextureFormat::kR8, TextureFormat::kRgba4};

// Returns a mip chain of a texture of "texel" in every texel of level 0, and
// zeros in every other level.
std::vector<uint8_t> MakeChain(TextureFormat format, int width, int height,
                               int mip_levels, uint32_t texel) {
  std::vector<uint8_t> chain(
      GetMipChainSizeInBytes(format, width, height, mip_levels), 0);
  const int texel_size = GetTexelSize(format);
  for (int i = 0; i < width * height; ++i) {
    std::memcpy(chain.data() + i * texel_size, &texel, texel_size);
  }
  return chain;
}

// Returns texel (x, y) of a mip level of a chain.
uint32_t GetTexel(TextureFormat format, const std::vector<uint8_t>& chain,
                  int width, int height, int mip, int x, int y) {
  const int texel_size = GetTexelSize(format);
  const int64_t offset = GetMipLevelOffset(format, width, height, mip) +
                         (y * (width >> mip) + x) * texel_size;
  uint32_t texel = 0;
  std::memcpy(&texel, chain.data() + offset, texel_size);
  return texel;
}

TEST(MipmapsTest, MipLevelCount) {
  EXPECT_EQ(GetMipLevelCount(64, 64, SamplerOptions().SetMipmap(false)), 1);
  EXPECT_EQ(GetMipLevelCount(64, 64, SamplerOptions()), 7);
  EXPECT_EQ(GetMipLevelCount(64, 32, SamplerOptions()), 6);
  EXPECT_EQ(GetMipLevelCount(64, 64, SamplerOptions().SetTileSize(16)), 5);
}

TEST(MipmapsTest, MipChainSizeInBytes) {
  EXPECT_EQ(GetMipChainSizeInBytes(TextureFormat::kR8, 4, 4, 3), 16 + 4 + 1);
  EXPECT_EQ(GetMipChainSizeInBytes(TextureFormat::kRgba4, 4, 4, 3),
            (16 + 4 + 1) * 2);
  EXPECT_EQ(GetMipChainSizeInBytes(TextureFormat::kRgba8, 8, 4, 3),
            (32 + 8 + 2) * 4);
}

TEST(MipmapsTest, UniformTextureKeepsItsTexelInEachFormat) {
  // Each channel has a different value, so a wrong channel count or order
  // shows.
  const Pixel pixel(16, 128, 204, 255);
  for (TextureFormat format : kAllFormats) {
    std::vector<uint8_t> chain =
        MakeChain(format, 8, 8, 4, PixelToTexel(format, pixel));
    const uint32_t texel = GetTexel(format, chain, 8, 8, 0, 0, 0);
    GenerateMipmaps(format, 8, 8, 4, /*tile_size=*/0, chain.data());
    for (int mip = 1; mip < 4; ++mip) {
      const int size = 8 >> mip;
      for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
          EXPECT_EQ(GetTexel(format, chain, 8, 8, mip, x, y), texel)
              << "format " << static_cast<int>(format) << ", mip " << mip
              << ", texel (" << x << ", " << y << ")";
        }
      }
    }
  }
}

TEST(MipmapsTest, TilesDoNotBlend) {
  // A 4x4 texture of four 2x2 tiles, each a different color.
  const TextureFormat format = TextureFormat::kRgba8;
  const Pixel colors[] = {Pixel(255, 0, 0), Pixel(0, 255, 0), Pixel(0, 0, 255),
                          Pixel(255, 255, 255)};
  std::vector<uint8_t> chain = MakeChain(format, 4, 4, 2, 0);
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      const uint32_t texel = colors[(y / 2) * 2 + x / 2].Packed();
      std::memcpy(chain.data() + (y * 4 + x) * 4, &texel, 4);
    }
  }
  GenerateMipmaps(format, 4, 4, 2, /*tile_size=*/2, chain.data());
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      EXPECT_EQ(GetTexel(format, chain, 4, 4, 1, x, y),
                colors[y * 2 + x].Packed())
          << "texel (" << x << ", " << y << ")";
    }
  }
}

TEST(MipmapsTest, ChannelsResizeIndependently) {
  // Red is bright on the left half and green on the right, so the level below
  // has more red on the left, and more green on the right.
  const TextureFormat format = TextureFormat::kRg8;
  std::vector<uint8_t> chain = MakeChain(format, 4, 4, 2, 0);
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      chain[(y * 4 + x) * 2] = x < 2 ? 255 : 0;
      chain[(y * 4 + x) * 2 + 1] = x < 2 ? 0 : 255;
    }
  }
  GenerateMipmaps(format, 4, 4, 2, /*tile_size=*/0, chain.data());
  const uint32_t left = GetTexel(format, chain, 4, 4, 1, 0, 0);
  const uint32_t right = GetTexel(format, chain, 4, 4, 1, 1, 0);
  EXPECT_GT(left & 0xFF, right & 0xFF);
  EXPECT_LT(left >> 8, right >> 8);
}

}  // namespace
}  // namespace gb
