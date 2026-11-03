// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/volume_texture.h"

#include <cstdint>
#include <tuple>
#include <vector>

#include "gb/render/render_test.h"
#include "gb/render/test_volume_texture.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Each;
using ::testing::ElementsAreArray;

constexpr int kWidth = 4;
constexpr int kHeight = 3;
constexpr int kDepth = 2;
constexpr int kTexelCount = kWidth * kHeight * kDepth;

// Volume textures have no mipmaps.
SamplerOptions VolumeOptions() { return SamplerOptions().SetMipmap(false); }

// Returns the index of a texel in a volume of kWidth by kHeight by kDepth.
int TexelIndex(int x, int y, int z) { return (z * kHeight + y) * kWidth + x; }

// Returns kTexelCount kR8 texels, each its own index plus "base".
std::vector<uint8_t> MakeTexels(int base = 0) {
  std::vector<uint8_t> texels(kTexelCount);
  for (int i = 0; i < kTexelCount; ++i) {
    texels[i] = static_cast<uint8_t>(base + i);
  }
  return texels;
}

class VolumeTextureTest : public RenderTest {
 protected:
  // Creates a kR8 volume texture of kWidth by kHeight by kDepth.
  ResourcePtr<VolumeTexture> CreateVolume(
      DataVolatility volatility = DataVolatility::kStaticReadWrite) {
    return render_system_->CreateVolumeTexture(volatility, kWidth, kHeight,
                                               kDepth, VolumeOptions(),
                                               TextureFormat::kR8);
  }
};

TEST_F(VolumeTextureTest, CreateAsResourcePtr) {
  CreateSystem();
  ResourcePtr<VolumeTexture> volume_texture = CreateVolume();
  ASSERT_NE(volume_texture, nullptr);
  auto* test_volume_texture =
      static_cast<TestVolumeTexture*>(volume_texture.Get());

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_volume_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_volume_texture->GetModifyCount(), 0);
}

TEST_F(VolumeTextureTest, CreateInResourceSet) {
  CreateSystem();
  ResourceSet resource_set;
  VolumeTexture* volume_texture = render_system_->CreateVolumeTexture(
      &resource_set, DataVolatility::kStaticWrite, kWidth, kHeight, kDepth,
      VolumeOptions());
  ASSERT_NE(volume_texture, nullptr);
  EXPECT_EQ(resource_set.Get<VolumeTexture>(volume_texture->GetResourceId()),
            volume_texture);
}

TEST_F(VolumeTextureTest, Properties) {
  CreateSystem();
  const SamplerOptions options =
      VolumeOptions().SetFilter(false).SetAddressMode(
          SamplerAddressMode::kClampEdge);
  auto volume_texture = render_system_->CreateVolumeTexture(
      DataVolatility::kPerFrame, kWidth, kHeight, kDepth, options,
      TextureFormat::kRg8);
  ASSERT_NE(volume_texture, nullptr);

  EXPECT_EQ(volume_texture->GetVolatility(), DataVolatility::kPerFrame);
  EXPECT_EQ(volume_texture->GetWidth(), kWidth);
  EXPECT_EQ(volume_texture->GetHeight(), kHeight);
  EXPECT_EQ(volume_texture->GetDepth(), kDepth);
  EXPECT_EQ(volume_texture->GetFormat(), TextureFormat::kRg8);
  EXPECT_EQ(volume_texture->GetSamplerOptions(), options);
}

TEST_F(VolumeTextureTest, CreateValidatesSize) {
  CreateSystem();
  for (const auto& [width, height, depth] :
       {std::tuple{kMaxVolumeTextureSize, 1, 1},
        std::tuple{1, kMaxVolumeTextureSize, 1},
        std::tuple{1, 1, kMaxVolumeTextureSize}}) {
    EXPECT_NE(render_system_->CreateVolumeTexture(
                  DataVolatility::kStaticWrite, width, height, depth,
                  VolumeOptions(), TextureFormat::kR8),
              nullptr)
        << width << "x" << height << "x" << depth;
  }
  for (const auto& [width, height, depth] :
       {std::tuple{kMaxVolumeTextureSize + 1, 1, 1},
        std::tuple{1, kMaxVolumeTextureSize + 1, 1},
        std::tuple{1, 1, kMaxVolumeTextureSize + 1}, std::tuple{0, 1, 1},
        std::tuple{1, 0, 1}, std::tuple{1, 1, 0}, std::tuple{1, 1, -1},
        // One slice more than kMaxVolumeTextureTexels allows.
        std::tuple{kMaxVolumeTextureSize, kMaxVolumeTextureSize,
                   kMaxVolumeTextureTexels /
                           (kMaxVolumeTextureSize * kMaxVolumeTextureSize) +
                       1}}) {
    EXPECT_EQ(render_system_->CreateVolumeTexture(
                  DataVolatility::kStaticWrite, width, height, depth,
                  VolumeOptions(), TextureFormat::kR8),
              nullptr)
        << width << "x" << height << "x" << depth;
  }

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(VolumeTextureTest, CreateWithoutMipmapsOrTiles) {
  CreateSystem();
  EXPECT_EQ(
      render_system_->CreateVolumeTexture(DataVolatility::kStaticWrite, kWidth,
                                          kHeight, kDepth, SamplerOptions()),
      nullptr);
  EXPECT_EQ(render_system_->CreateVolumeTexture(DataVolatility::kStaticWrite,
                                                kWidth, kHeight, kDepth,
                                                VolumeOptions().SetTileSize(2)),
            nullptr);
  EXPECT_NE(
      render_system_->CreateVolumeTexture(DataVolatility::kStaticWrite, kWidth,
                                          kHeight, kDepth, VolumeOptions()),
      nullptr);
}

TEST_F(VolumeTextureTest, CreateInEachFormat) {
  CreateSystem();
  for (TextureFormat format :
       {TextureFormat::kRgba8Srgb, TextureFormat::kRgba8, TextureFormat::kRg8,
        TextureFormat::kR8, TextureFormat::kRgba4}) {
    auto volume_texture = render_system_->CreateVolumeTexture(
        DataVolatility::kStaticReadWrite, kWidth, kHeight, kDepth,
        VolumeOptions(), format);
    ASSERT_NE(volume_texture, nullptr) << static_cast<int>(format);
    EXPECT_EQ(volume_texture->GetFormat(), format);
    EXPECT_EQ(static_cast<TestVolumeTexture*>(volume_texture.Get())
                  ->GetTexels()
                  .size(),
              GetTextureSizeInBytes(format, kWidth, kHeight, kDepth));
  }
  EXPECT_EQ(render_system_->CreateVolumeTexture(
                DataVolatility::kStaticReadWrite, kWidth, kHeight, kDepth,
                VolumeOptions(), static_cast<TextureFormat>(100)),
            nullptr);
}

TEST_F(VolumeTextureTest, FilterOnlyFilterableFormats) {
  state_.unfilterable_formats.insert(TextureFormat::kR8);
  CreateSystem();
  EXPECT_EQ(render_system_->CreateVolumeTexture(
                DataVolatility::kStaticWrite, kWidth, kHeight, kDepth,
                VolumeOptions().SetFilter(true), TextureFormat::kR8),
            nullptr);
  EXPECT_NE(render_system_->CreateVolumeTexture(
                DataVolatility::kStaticWrite, kWidth, kHeight, kDepth,
                VolumeOptions().SetFilter(false), TextureFormat::kR8),
            nullptr);
  EXPECT_NE(render_system_->CreateVolumeTexture(
                DataVolatility::kStaticWrite, kWidth, kHeight, kDepth,
                VolumeOptions().SetFilter(true), TextureFormat::kRg8),
            nullptr);
}

TEST_F(VolumeTextureTest, FailCreate) {
  state_.fail_create_volume_texture = true;
  CreateSystem();
  EXPECT_EQ(CreateVolume(), nullptr);
}

TEST_F(VolumeTextureTest, Clear) {
  CreateSystem();
  auto volume_texture = CreateVolume();
  ASSERT_NE(volume_texture, nullptr);
  auto* test_volume_texture =
      static_cast<TestVolumeTexture*>(volume_texture.Get());

  EXPECT_TRUE(volume_texture->Clear());
  EXPECT_THAT(test_volume_texture->GetTexels(), Each(0));
  EXPECT_TRUE(volume_texture->Clear(Pixel(12, 34, 56, 78)));
  EXPECT_THAT(test_volume_texture->GetTexels(), Each(12));

  EXPECT_EQ(test_volume_texture->GetModifyCount(), 2);
  EXPECT_EQ(test_volume_texture->GetInvalidCallCount(), 0);
}

TEST_F(VolumeTextureTest, ClearConvertsToFormat) {
  CreateSystem();
  auto volume_texture = render_system_->CreateVolumeTexture(
      DataVolatility::kStaticReadWrite, kWidth, kHeight, kDepth,
      VolumeOptions(), TextureFormat::kRgba4);
  ASSERT_NE(volume_texture, nullptr);

  EXPECT_TRUE(volume_texture->Clear(Pixel(0x12, 0x34, 0x56, 0x78)));
  std::vector<uint16_t> texels(kTexelCount);
  ASSERT_TRUE(volume_texture->Get(texels.data(), kTexelCount * 2));
  EXPECT_THAT(texels, Each(0x1357));
}

TEST_F(VolumeTextureTest, ClearRegion) {
  CreateSystem();
  auto volume_texture = CreateVolume();
  ASSERT_NE(volume_texture, nullptr);
  auto* test_volume_texture =
      static_cast<TestVolumeTexture*>(volume_texture.Get());
  ASSERT_TRUE(volume_texture->Clear());

  // The region is clipped to x in [2, 4), y in [0, 2), and z in [1, 2).
  EXPECT_TRUE(volume_texture->ClearRegion(2, -1, 1, 5, 3, 5, Pixel(9, 0, 0)));
  std::vector<uint8_t> expected(kTexelCount, 0);
  for (int y = 0; y < 2; ++y) {
    for (int x = 2; x < 4; ++x) {
      expected[TexelIndex(x, y, 1)] = 9;
    }
  }
  EXPECT_THAT(test_volume_texture->GetTexels(), ElementsAreArray(expected));
  EXPECT_EQ(test_volume_texture->GetInvalidCallCount(), 0);
}

TEST_F(VolumeTextureTest, ClearRegionOutOfBounds) {
  CreateSystem();
  auto volume_texture = CreateVolume();
  ASSERT_NE(volume_texture, nullptr);
  auto* test_volume_texture =
      static_cast<TestVolumeTexture*>(volume_texture.Get());

  EXPECT_TRUE(volume_texture->ClearRegion(kWidth, 0, 0, 1, 1, 1));
  EXPECT_TRUE(volume_texture->ClearRegion(0, -1, 0, 1, 1, 1));
  EXPECT_TRUE(volume_texture->ClearRegion(0, 0, kDepth, 1, 1, 1));
  EXPECT_TRUE(volume_texture->ClearRegion(0, 0, 0, 1, 1, 0));
  EXPECT_EQ(test_volume_texture->GetModifyCount(), 0);
}

TEST_F(VolumeTextureTest, Set) {
  CreateSystem();
  auto volume_texture = CreateVolume();
  ASSERT_NE(volume_texture, nullptr);
  auto* test_volume_texture =
      static_cast<TestVolumeTexture*>(volume_texture.Get());

  const std::vector<uint8_t> texels = MakeTexels();
  EXPECT_TRUE(volume_texture->Set(texels.data(), texels.size()));
  EXPECT_THAT(test_volume_texture->GetTexels(), ElementsAreArray(texels));

  EXPECT_FALSE(volume_texture->Set(texels.data(), texels.size() - 1));
  EXPECT_EQ(test_volume_texture->GetModifyCount(), 1);
  EXPECT_EQ(test_volume_texture->GetInvalidCallCount(), 0);
}

TEST_F(VolumeTextureTest, SetRegion) {
  CreateSystem();
  auto volume_texture = CreateVolume();
  ASSERT_NE(volume_texture, nullptr);
  auto* test_volume_texture =
      static_cast<TestVolumeTexture*>(volume_texture.Get());
  ASSERT_TRUE(volume_texture->Clear());

  // A region of 3 by 2 by 2 texels, of which x in [0, 2), y in [1, 2), and z
  // in [1, 2) are in the volume.
  std::vector<uint8_t> region(3 * 2 * 2);
  for (int i = 0; i < static_cast<int>(region.size()); ++i) {
    region[i] = static_cast<uint8_t>(100 + i);
  }
  EXPECT_TRUE(volume_texture->SetRegion(-1, kHeight - 1, kDepth - 1, 3, 2, 2,
                                        region.data(), region.size()));

  // The texels in the volume are region (1, 0, 0) and (2, 0, 0).
  std::vector<uint8_t> expected(kTexelCount, 0);
  expected[TexelIndex(0, kHeight - 1, kDepth - 1)] = 101;
  expected[TexelIndex(1, kHeight - 1, kDepth - 1)] = 102;
  EXPECT_THAT(test_volume_texture->GetTexels(), ElementsAreArray(expected));

  // Clipping the start of each axis skips the region's first texels in each.
  ASSERT_TRUE(volume_texture->Clear());
  EXPECT_TRUE(volume_texture->SetRegion(-2, -1, -1, 3, 2, 2, region.data(),
                                        region.size()));
  expected.assign(kTexelCount, 0);
  expected[TexelIndex(0, 0, 0)] = region[(1 * 2 + 1) * 3 + 2];
  EXPECT_THAT(test_volume_texture->GetTexels(), ElementsAreArray(expected));
  EXPECT_EQ(test_volume_texture->GetInvalidCallCount(), 0);
}

TEST_F(VolumeTextureTest, SetRegionValidatesSize) {
  CreateSystem();
  auto volume_texture = CreateVolume();
  ASSERT_NE(volume_texture, nullptr);
  auto* test_volume_texture =
      static_cast<TestVolumeTexture*>(volume_texture.Get());

  const std::vector<uint8_t> texels = MakeTexels();
  EXPECT_FALSE(volume_texture->SetRegion(0, 0, 0, 2, 2, 2, texels.data(), 7));
  EXPECT_TRUE(volume_texture->SetRegion(0, 0, 0, 2, 2, 2, texels.data(), 8));

  // Regions with nothing in the volume succeed, writing nothing.
  EXPECT_TRUE(volume_texture->SetRegion(0, 0, 0, 0, 2, 2, texels.data(), 0));
  EXPECT_TRUE(
      volume_texture->SetRegion(0, 0, kDepth, 2, 2, 2, texels.data(), 8));
  EXPECT_EQ(test_volume_texture->GetModifyCount(), 1);
}

TEST_F(VolumeTextureTest, Get) {
  CreateSystem();
  auto volume_texture = CreateVolume();
  ASSERT_NE(volume_texture, nullptr);

  const std::vector<uint8_t> texels = MakeTexels(10);
  ASSERT_TRUE(volume_texture->Set(texels.data(), texels.size()));
  std::vector<uint8_t> out_texels(kTexelCount);
  EXPECT_TRUE(volume_texture->Get(out_texels.data(), out_texels.size()));
  EXPECT_EQ(out_texels, texels);

  EXPECT_FALSE(volume_texture->Get(out_texels.data(), out_texels.size() - 1));
}

TEST_F(VolumeTextureTest, CannotGetStaticWrite) {
  CreateSystem();
  auto volume_texture = CreateVolume(DataVolatility::kStaticWrite);
  ASSERT_NE(volume_texture, nullptr);

  std::vector<uint8_t> out_texels(kTexelCount);
  EXPECT_FALSE(volume_texture->Get(out_texels.data(), out_texels.size()));
}

TEST_F(VolumeTextureTest, BackendFailures) {
  CreateSystem();
  auto volume_texture = CreateVolume();
  ASSERT_NE(volume_texture, nullptr);
  const std::vector<uint8_t> texels = MakeTexels();
  std::vector<uint8_t> out_texels(kTexelCount);

  state_.volume_texture_config.fail_clear = true;
  EXPECT_FALSE(volume_texture->Clear());
  state_.volume_texture_config.fail_set = true;
  EXPECT_FALSE(volume_texture->Set(texels.data(), texels.size()));
  state_.volume_texture_config.fail_get = true;
  EXPECT_FALSE(volume_texture->Get(out_texels.data(), out_texels.size()));
}

}  // namespace
}  // namespace gb
