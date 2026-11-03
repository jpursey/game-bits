// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/texture.h"

#include <limits>
#include <vector>

#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Each;
using ::testing::ElementsAreArray;

constexpr int kTextureWidth = 16;
constexpr int kTextureHeight = 32;

class TextureTest : public RenderTest {
 protected:
  std::vector<Pixel> MakePixels(int count, Pixel value,
                                Pixel increment = {0, 0, 0, 0}) {
    std::vector<Pixel> pixels(count, value);
    if (increment.Packed() != 0) {
      for (int i = 1; i < count; ++i) {
        pixels[i].r = static_cast<uint8_t>((increment.r * i) & 0xFF);
        pixels[i].g = static_cast<uint8_t>((increment.g * i) & 0xFF);
        pixels[i].b = static_cast<uint8_t>((increment.b * i) & 0xFF);
        pixels[i].a = static_cast<uint8_t>((increment.a * i) & 0xFF);
      }
    }
    return pixels;
  }

  std::vector<uint32_t> MakePackedPixels(int count, uint32_t value,
                                         uint32_t increment = 0) {
    std::vector<uint32_t> pixels(count, value);
    if (increment != 0) {
      for (int i = 1; i < count; ++i) {
        pixels[i] = increment * i;
      }
    }
    return pixels;
  }
};

TEST_F(TextureTest, CreateAsResourcePtr) {
  CreateSystem();
  ResourcePtr<Texture> texture = render_system_->CreateTexture(
      DataVolatility::kStaticWrite, kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);
}

TEST_F(TextureTest, CreateInResourceSet) {
  CreateSystem();
  ResourceSet resource_set;
  Texture* texture =
      render_system_->CreateTexture(&resource_set, DataVolatility::kStaticWrite,
                                    kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture);
  EXPECT_EQ(resource_set.Get<Texture>(texture->GetResourceId()), texture);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);
}

TEST_F(TextureTest, CreateAtMaxSize) {
  CreateSystem();
  EXPECT_NE(render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                          kMaxTextureWidth, 1),
            nullptr);
  EXPECT_NE(render_system_->CreateTexture(DataVolatility::kStaticWrite, 1,
                                          kMaxTextureHeight),
            nullptr);
  EXPECT_EQ(render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                          kMaxTextureWidth + 1, 1),
            nullptr);
  EXPECT_EQ(render_system_->CreateTexture(DataVolatility::kStaticWrite, 1,
                                          kMaxTextureHeight + 1),
            nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(TextureTest, Properties) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  EXPECT_EQ(texture->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(texture->GetWidth(), kTextureWidth);
  EXPECT_EQ(texture->GetHeight(), kTextureHeight);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);
}

TEST_F(TextureTest, Clear) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  EXPECT_TRUE(texture->Clear());
  EXPECT_THAT(test_texture->GetPixels(), Each(Pixel(0, 0, 0, 0)));

  EXPECT_TRUE(texture->Clear(Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture->GetPixels(), Each(Pixel(1, 2, 3, 4)));

  EXPECT_TRUE(texture->Clear(0xdeadbeef));
  EXPECT_THAT(test_texture->GetPackedPixels(), Each(0xdeadbeef));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 3);
}

TEST_F(TextureTest, FailClear) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  state_.texture_config.fail_clear = true;
  EXPECT_FALSE(texture->Clear());
  EXPECT_FALSE(texture->Clear(Pixel(1, 2, 3, 4)));
  EXPECT_FALSE(texture->Clear(0xdeadbeef));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);
}

TEST_F(TextureTest, Set) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  auto pixels =
      MakePixels(kTextureWidth * kTextureHeight, {0, 0, 0, 0}, {1, 2, 3, 4});
  auto packed_pixels = MakePackedPixels(kTextureWidth * kTextureHeight, 0, 1);

  EXPECT_TRUE(texture->Set(pixels));
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(pixels));

  EXPECT_TRUE(texture->Set(packed_pixels));
  EXPECT_THAT(test_texture->GetPackedPixels(), ElementsAreArray(packed_pixels));

  EXPECT_TRUE(texture->Set(pixels.data(), pixels.size() * sizeof(Pixel)));
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(pixels));

  auto oversize_pixels = MakePixels(kTextureWidth * kTextureHeight + 1,
                                    {0, 0, 0, 0}, {1, 2, 3, 4});
  auto oversize_packed_pixels =
      MakePackedPixels(kTextureWidth * kTextureHeight + 1, 0, 1);
  ASSERT_TRUE(texture->Clear());

  EXPECT_TRUE(texture->Set(oversize_pixels));
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(pixels));

  EXPECT_TRUE(texture->Set(oversize_packed_pixels));
  EXPECT_THAT(test_texture->GetPackedPixels(), ElementsAreArray(packed_pixels));

  EXPECT_TRUE(texture->Set(oversize_pixels.data(),
                           oversize_pixels.size() * sizeof(Pixel)));
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(pixels));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 7);
}

TEST_F(TextureTest, FailSet) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  EXPECT_FALSE(
      texture->Set(MakePixels(kTextureWidth * kTextureHeight - 1, {})));
  EXPECT_FALSE(
      texture->Set(MakePackedPixels(kTextureWidth * kTextureHeight - 1, 0)));
  auto pixels = MakePixels(kTextureWidth * kTextureHeight, {});
  EXPECT_FALSE(texture->Set(pixels.data(), pixels.size() * sizeof(Pixel) - 1));

  state_.texture_config.fail_set = true;
  EXPECT_FALSE(texture->Set(MakePixels(kTextureWidth * kTextureHeight, {})));
  EXPECT_FALSE(
      texture->Set(MakePackedPixels(kTextureWidth * kTextureHeight, 0)));
  EXPECT_FALSE(texture->Set(pixels.data(), pixels.size() * sizeof(Pixel)));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);
}

TEST_F(TextureTest, ClearRegion) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  // None of these trigger an actual clear, as the regions are zero or fully
  // clipped.
  EXPECT_TRUE(texture->ClearRegion(0, 0, 0, 1));
  EXPECT_TRUE(texture->ClearRegion(0, 0, 1, 0));
  EXPECT_TRUE(texture->ClearRegion(10, 10, -1, 1));
  EXPECT_TRUE(texture->ClearRegion(10, 10, 1, -1));
  EXPECT_TRUE(texture->ClearRegion(-1, 0, 1, 1));
  EXPECT_TRUE(texture->ClearRegion(0, -1, 1, 1));
  EXPECT_TRUE(texture->ClearRegion(kTextureWidth, 0, 1, 1));
  EXPECT_TRUE(texture->ClearRegion(0, kTextureHeight, 1, 1));

  EXPECT_TRUE(texture->ClearRegion(0, 0, 0, 1, Pixel(1, 2, 3, 4)));
  EXPECT_TRUE(texture->ClearRegion(0, 0, 1, 0, Pixel(1, 2, 3, 4)));
  EXPECT_TRUE(texture->ClearRegion(10, 10, -1, 1, Pixel(1, 2, 3, 4)));
  EXPECT_TRUE(texture->ClearRegion(10, 10, 1, -1, Pixel(1, 2, 3, 4)));
  EXPECT_TRUE(texture->ClearRegion(-1, 0, 1, 1, Pixel(1, 2, 3, 4)));
  EXPECT_TRUE(texture->ClearRegion(0, -1, 1, 1, Pixel(1, 2, 3, 4)));
  EXPECT_TRUE(texture->ClearRegion(kTextureWidth, 0, 1, 1, Pixel(1, 2, 3, 4)));
  EXPECT_TRUE(texture->ClearRegion(0, kTextureHeight, 1, 1, Pixel(1, 2, 3, 4)));

  EXPECT_TRUE(texture->ClearRegion(0, 0, 0, 1, 0xdeadbeef));
  EXPECT_TRUE(texture->ClearRegion(0, 0, 1, 0, 0xdeadbeef));
  EXPECT_TRUE(texture->ClearRegion(10, 10, -1, 1, 0xdeadbeef));
  EXPECT_TRUE(texture->ClearRegion(10, 10, 1, -1, 0xdeadbeef));
  EXPECT_TRUE(texture->ClearRegion(-1, 0, 1, 1, 0xdeadbeef));
  EXPECT_TRUE(texture->ClearRegion(0, -1, 1, 1, 0xdeadbeef));
  EXPECT_TRUE(texture->ClearRegion(kTextureWidth, 0, 1, 1, 0xdeadbeef));
  EXPECT_TRUE(texture->ClearRegion(0, kTextureHeight, 1, 1, 0xdeadbeef));

  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);

  // Clear entire texture
  EXPECT_TRUE(texture->ClearRegion(0, 0, kTextureWidth, kTextureHeight));
  EXPECT_THAT(test_texture->GetPixels(), Each(Pixel(0, 0, 0, 0)));
  EXPECT_TRUE(texture->ClearRegion(0, 0, kTextureWidth, kTextureHeight,
                                   Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture->GetPixels(), Each(Pixel(1, 2, 3, 4)));
  EXPECT_TRUE(
      texture->ClearRegion(0, 0, kTextureWidth, kTextureHeight, 0xdeadbeef));
  EXPECT_THAT(test_texture->GetPackedPixels(), Each(0xdeadbeef));
  ASSERT_TRUE(texture->Clear(0xFFFFFFFF));

  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 4);

  // Clear with top,left clipping
  EXPECT_TRUE(texture->ClearRegion(-10, -10, 20, 20));
  EXPECT_THAT(test_texture->GetPixelRegion(0, 0, 10, 10),
              Each(Pixel(0, 0, 0, 0)));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(10, 0, kTextureWidth - 10,
                                                 kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(
      test_texture->GetPackedPixelRegion(0, 10, 10, kTextureHeight - 10),
      Each(0xFFFFFFFF));
  EXPECT_TRUE(texture->ClearRegion(-10, -10, 20, 20, Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture->GetPixelRegion(0, 0, 10, 10),
              Each(Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(10, 0, kTextureWidth - 10,
                                                 kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(
      test_texture->GetPackedPixelRegion(0, 10, 10, kTextureHeight - 10),
      Each(0xFFFFFFFF));
  EXPECT_TRUE(texture->ClearRegion(-10, -10, 20, 20, 0xdeadbeef));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 0, 10, 10),
              Each(0xdeadbeef));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(10, 0, kTextureWidth - 10,
                                                 kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(
      test_texture->GetPackedPixelRegion(0, 10, 10, kTextureHeight - 10),
      Each(0xFFFFFFFF));
  ASSERT_TRUE(texture->Clear(0xFFFFFFFF));

  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 8);

  // Clear with bottom,right clipping
  EXPECT_TRUE(texture->ClearRegion(10, 10, kTextureWidth, kTextureHeight));
  EXPECT_THAT(test_texture->GetPixelRegion(10, 10, kTextureWidth - 10,
                                           kTextureHeight - 10),
              Each(Pixel(0, 0, 0, 0)));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 0, 10, kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(10, 0, kTextureWidth - 10, 10),
              Each(0xFFFFFFFF));
  EXPECT_TRUE(texture->ClearRegion(10, 10, kTextureWidth, kTextureHeight,
                                   Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture->GetPixelRegion(10, 10, kTextureWidth - 10,
                                           kTextureHeight - 10),
              Each(Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 0, 10, kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(10, 0, kTextureWidth - 10, 10),
              Each(0xFFFFFFFF));
  EXPECT_TRUE(
      texture->ClearRegion(10, 10, kTextureWidth, kTextureHeight, 0xdeadbeef));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(10, 10, kTextureWidth - 10,
                                                 kTextureHeight - 10),
              Each(0xdeadbeef));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 0, 10, kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(10, 0, kTextureWidth - 10, 10),
              Each(0xFFFFFFFF));
  ASSERT_TRUE(texture->Clear(0xFFFFFFFF));

  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 12);

  // Clear with clipping on all sides
  EXPECT_TRUE(
      texture->ClearRegion(-1, -1, kTextureWidth + 1, kTextureHeight + 1));
  EXPECT_THAT(test_texture->GetPixels(), Each(Pixel(0, 0, 0, 0)));
  EXPECT_TRUE(texture->ClearRegion(-1, -1, kTextureWidth + 1,
                                   kTextureHeight + 1, Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture->GetPixels(), Each(Pixel(1, 2, 3, 4)));
  EXPECT_TRUE(texture->ClearRegion(-1, -1, kTextureWidth + 1,
                                   kTextureHeight + 1, 0xdeadbeef));
  EXPECT_THAT(test_texture->GetPackedPixels(), Each(0xdeadbeef));

  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 15);
}

TEST_F(TextureTest, FailClearRegion) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  state_.texture_config.fail_clear = true;
  EXPECT_FALSE(texture->ClearRegion(0, 0, 1, 1));
  EXPECT_FALSE(texture->ClearRegion(0, 0, 1, 1, Pixel(1, 2, 3, 4)));
  EXPECT_FALSE(texture->ClearRegion(0, 0, 1, 1, 0xdeadbeef));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);
}

TEST_F(TextureTest, SetRegion) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  auto pixels =
      MakePixels(kTextureWidth * kTextureHeight, {0, 0, 0, 0}, {1, 2, 3, 4});
  auto packed_pixels = MakePackedPixels(kTextureWidth * kTextureHeight, 0, 1);

  // None of these trigger an actual set, as the regions are zero or fully
  // clipped.
  EXPECT_TRUE(texture->SetRegion(0, 0, 0, 1, pixels));
  EXPECT_TRUE(texture->SetRegion(0, 0, 1, 0, pixels));
  EXPECT_TRUE(texture->SetRegion(10, 10, -1, 1, pixels));
  EXPECT_TRUE(texture->SetRegion(10, 10, 1, -1, pixels));
  EXPECT_TRUE(texture->SetRegion(-1, 0, 1, 1, pixels));
  EXPECT_TRUE(texture->SetRegion(0, -1, 1, 1, pixels));
  EXPECT_TRUE(texture->SetRegion(kTextureWidth, 0, 1, 1, pixels));
  EXPECT_TRUE(texture->SetRegion(0, kTextureHeight, 1, 1, pixels));

  EXPECT_TRUE(texture->SetRegion(0, 0, 0, 1, packed_pixels));
  EXPECT_TRUE(texture->SetRegion(0, 0, 1, 0, packed_pixels));
  EXPECT_TRUE(texture->SetRegion(10, 10, -1, 1, packed_pixels));
  EXPECT_TRUE(texture->SetRegion(10, 10, 1, -1, packed_pixels));
  EXPECT_TRUE(texture->SetRegion(-1, 0, 1, 1, packed_pixels));
  EXPECT_TRUE(texture->SetRegion(0, -1, 1, 1, packed_pixels));
  EXPECT_TRUE(texture->SetRegion(kTextureWidth, 0, 1, 1, packed_pixels));
  EXPECT_TRUE(texture->SetRegion(0, kTextureHeight, 1, 1, packed_pixels));

  EXPECT_TRUE(texture->SetRegion(0, 0, 0, 1, pixels.data(),
                                 pixels.size() * sizeof(Pixel)));
  EXPECT_TRUE(texture->SetRegion(0, 0, 1, 0, pixels.data(),
                                 pixels.size() * sizeof(Pixel)));
  EXPECT_TRUE(texture->SetRegion(10, 10, -1, 1, pixels.data(),
                                 pixels.size() * sizeof(Pixel)));
  EXPECT_TRUE(texture->SetRegion(10, 10, 1, -1, pixels.data(),
                                 pixels.size() * sizeof(Pixel)));
  EXPECT_TRUE(texture->SetRegion(-1, 0, 1, 1, pixels.data(),
                                 pixels.size() * sizeof(Pixel)));
  EXPECT_TRUE(texture->SetRegion(0, -1, 1, 1, pixels.data(),
                                 pixels.size() * sizeof(Pixel)));
  EXPECT_TRUE(texture->SetRegion(kTextureWidth, 0, 1, 1, pixels.data(),
                                 pixels.size() * sizeof(Pixel)));
  EXPECT_TRUE(texture->SetRegion(0, kTextureHeight, 1, 1, pixels.data(),
                                 pixels.size() * sizeof(Pixel)));

  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);

  // Set entire texture
  EXPECT_TRUE(texture->SetRegion(0, 0, kTextureWidth, kTextureHeight, pixels));
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(pixels));
  EXPECT_TRUE(
      texture->SetRegion(0, 0, kTextureWidth, kTextureHeight, packed_pixels));
  EXPECT_THAT(test_texture->GetPackedPixels(), ElementsAreArray(packed_pixels));
  EXPECT_TRUE(texture->SetRegion(0, 0, kTextureWidth, kTextureHeight,
                                 pixels.data(), pixels.size() * sizeof(Pixel)));
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(pixels));
  ASSERT_TRUE(texture->Clear(0xFFFFFFFF));

  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 4);

  // Set with top,left clipping
  pixels = MakePixels(10 * 10, {0, 0, 0, 0}, {1, 2, 3, 4});
  packed_pixels = MakePackedPixels(10 * 10, 0, 1);
  std::vector<Pixel> clipped_pixels(5 * 5);
  std::vector<uint32_t> clipped_packed_pixels(5 * 5);
  for (int y = 0; y < 5; ++y) {
    for (int x = 0; x < 5; ++x) {
      clipped_pixels[y * 5 + x] = pixels[(y + 5) * 10 + (x + 5)];
      clipped_packed_pixels[y * 5 + x] = packed_pixels[(y + 5) * 10 + (x + 5)];
    }
  }
  EXPECT_TRUE(texture->SetRegion(-5, -5, 10, 10, pixels));
  EXPECT_THAT(test_texture->GetPixelRegion(0, 0, 5, 5),
              ElementsAreArray(clipped_pixels));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(5, 0, kTextureWidth - 5,
                                                 kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 5, 5, kTextureHeight - 5),
              Each(0xFFFFFFFF));
  EXPECT_TRUE(texture->SetRegion(-5, -5, 10, 10, packed_pixels));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 0, 5, 5),
              ElementsAreArray(clipped_packed_pixels));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(5, 0, kTextureWidth - 5,
                                                 kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 5, 5, kTextureHeight - 5),
              Each(0xFFFFFFFF));
  EXPECT_TRUE(texture->SetRegion(-5, -5, 10, 10, pixels.data(),
                                 pixels.size() * sizeof(Pixel)));
  EXPECT_THAT(test_texture->GetPixelRegion(0, 0, 5, 5),
              ElementsAreArray(clipped_pixels));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(5, 0, kTextureWidth - 5,
                                                 kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 5, 5, kTextureHeight - 5),
              Each(0xFFFFFFFF));
  ASSERT_TRUE(texture->Clear(0xFFFFFFFF));

  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 8);

  // Set with bottom,right clipping
  for (int y = 0; y < 5; ++y) {
    for (int x = 0; x < 5; ++x) {
      clipped_pixels[y * 5 + x] = pixels[y * 10 + x];
      clipped_packed_pixels[y * 5 + x] = packed_pixels[y * 10 + x];
    }
  }
  EXPECT_TRUE(texture->SetRegion(kTextureWidth - 5, kTextureHeight - 5, 10, 10,
                                 pixels));
  EXPECT_THAT(
      test_texture->GetPixelRegion(kTextureWidth - 5, kTextureHeight - 5, 5, 5),
      ElementsAreArray(clipped_pixels));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 0, kTextureWidth - 5,
                                                 kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(5, 0, kTextureWidth - 5, 5),
              Each(0xFFFFFFFF));
  EXPECT_TRUE(texture->SetRegion(kTextureWidth - 5, kTextureHeight - 5, 10, 10,
                                 packed_pixels));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(kTextureWidth - 5,
                                                 kTextureHeight - 5, 5, 5),
              ElementsAreArray(clipped_packed_pixels));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 0, kTextureWidth - 5,
                                                 kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(5, 0, kTextureWidth - 5, 5),
              Each(0xFFFFFFFF));
  EXPECT_TRUE(texture->SetRegion(kTextureWidth - 5, kTextureHeight - 5, 10, 10,
                                 pixels.data(), pixels.size() * sizeof(Pixel)));
  EXPECT_THAT(
      test_texture->GetPixelRegion(kTextureWidth - 5, kTextureHeight - 5, 5, 5),
      ElementsAreArray(clipped_pixels));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(0, 0, kTextureWidth - 5,
                                                 kTextureHeight),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture->GetPackedPixelRegion(5, 0, kTextureWidth - 5, 5),
              Each(0xFFFFFFFF));
  ASSERT_TRUE(texture->Clear(0xFFFFFFFF));

  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 12);

  // Set with clipping on all sides
  pixels = MakePixels((kTextureWidth + 2) * (kTextureHeight + 2), {0, 0, 0, 0},
                      {1, 2, 3, 4});
  packed_pixels =
      MakePackedPixels((kTextureWidth + 2) * (kTextureHeight + 2), 0, 1);
  clipped_pixels.resize(kTextureWidth * kTextureHeight);
  clipped_packed_pixels.resize(kTextureWidth * kTextureHeight);
  for (int y = 0; y < kTextureHeight; ++y) {
    for (int x = 0; x < kTextureWidth; ++x) {
      clipped_pixels[y * kTextureWidth + x] =
          pixels[(y + 1) * (kTextureWidth + 2) + (x + 1)];
      clipped_packed_pixels[y * kTextureWidth + x] =
          packed_pixels[(y + 1) * (kTextureWidth + 2) + (x + 1)];
    }
  }
  EXPECT_TRUE(texture->SetRegion(-1, -1, kTextureWidth + 2, kTextureHeight + 2,
                                 pixels));
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(clipped_pixels));
  EXPECT_TRUE(texture->SetRegion(-1, -1, kTextureWidth + 2, kTextureHeight + 2,
                                 packed_pixels));
  EXPECT_THAT(test_texture->GetPackedPixels(),
              ElementsAreArray(clipped_packed_pixels));
  EXPECT_TRUE(texture->SetRegion(-1, -1, kTextureWidth + 2, kTextureHeight + 2,
                                 pixels.data(), pixels.size() * sizeof(Pixel)));
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(clipped_pixels));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 15);
}

TEST_F(TextureTest, FailSetRegion) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  auto pixels = MakePixels(10 * 10, {});
  auto too_few_pixels = MakePixels(10 * 10 - 1, {});
  auto too_few_packed_pixels = MakePackedPixels(10 * 10 - 1, 0);
  EXPECT_FALSE(texture->SetRegion(0, 0, 10, 10, too_few_pixels));
  EXPECT_FALSE(texture->SetRegion(0, 0, 10, 10, too_few_packed_pixels));
  EXPECT_FALSE(texture->SetRegion(0, 0, 10, 10, pixels.data(),
                                  pixels.size() * sizeof(Pixel) - 1));
  EXPECT_FALSE(texture->SetRegion(-5, -5, 10, 10, too_few_pixels));
  EXPECT_FALSE(texture->SetRegion(-5, -5, 10, 10, too_few_packed_pixels));
  EXPECT_FALSE(texture->SetRegion(-5, -5, 10, 10, pixels.data(),
                                  pixels.size() * sizeof(Pixel) - 1));
  EXPECT_FALSE(texture->SetRegion(kTextureWidth - 5, kTextureHeight - 5, 10, 10,
                                  too_few_pixels));
  EXPECT_FALSE(texture->SetRegion(kTextureWidth - 5, kTextureHeight - 5, 10, 10,
                                  too_few_packed_pixels));
  EXPECT_FALSE(texture->SetRegion(kTextureWidth - 5, kTextureHeight - 5, 10, 10,
                                  pixels.data(),
                                  pixels.size() * sizeof(Pixel) - 1));

  state_.texture_config.fail_set = true;
  EXPECT_FALSE(texture->SetRegion(0, 0, 1, 1, MakePixels(1, {})));
  EXPECT_FALSE(texture->SetRegion(0, 0, 1, 1, MakePackedPixels(1, 0)));
  EXPECT_FALSE(texture->SetRegion(0, 0, 1, 1, pixels.data(), sizeof(Pixel)));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);
}

TEST_F(TextureTest, EditPreventsModification) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticReadWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  auto texture_view = texture->Edit();
  ASSERT_NE(texture_view, nullptr);

  EXPECT_FALSE(texture->Clear());
  EXPECT_FALSE(texture->Clear(Pixel(1, 2, 3, 4)));
  EXPECT_FALSE(texture->Clear(0xdeadbeef));
  EXPECT_FALSE(texture->Set(MakePixels(kTextureWidth * kTextureHeight, {})));
  EXPECT_FALSE(
      texture->Set(MakePackedPixels(kTextureWidth * kTextureHeight, {})));
  auto pixels = MakePixels(kTextureWidth * kTextureHeight, {});
  EXPECT_FALSE(texture->Set(pixels.data(), pixels.size() * sizeof(Pixel)));
  EXPECT_FALSE(texture->ClearRegion(0, 0, 1, 1));
  EXPECT_FALSE(texture->ClearRegion(0, 0, 1, 1, Pixel(1, 2, 3, 4)));
  EXPECT_FALSE(texture->ClearRegion(0, 0, 1, 1, 0xdeadbeef));
  EXPECT_FALSE(texture->SetRegion(0, 0, 1, 1, MakePixels(1, {})));
  EXPECT_FALSE(texture->SetRegion(0, 0, 1, 1, MakePackedPixels(1, 0)));
  EXPECT_FALSE(texture->SetRegion(0, 0, 1, 1, pixels.data(), sizeof(Pixel)));
  EXPECT_EQ(texture->Edit(), nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);
}

TEST_F(TextureTest, CannotEditStaticWrite) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  EXPECT_EQ(texture->Edit(), nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);
}

TEST_F(TextureTest, FailEdit) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticReadWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  state_.texture_config.fail_edit_begin = true;
  EXPECT_EQ(texture->Edit(), nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture->GetModifyCount(), 0);
}

TEST_F(TextureTest, Edit) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticReadWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  ASSERT_TRUE(texture->Clear());
  const int modify_count = test_texture->GetModifyCount();

  auto texture_view = texture->Edit();
  ASSERT_NE(texture_view, nullptr);
  EXPECT_EQ(texture_view->GetWidth(), kTextureWidth);
  EXPECT_EQ(texture_view->GetHeight(), kTextureHeight);
  texture_view->Modify(3, 5) = Pixel(1, 2, 3, 4);
  texture_view.reset();
  EXPECT_EQ(test_texture->GetPixel(3, 5), Pixel(1, 2, 3, 4));
  EXPECT_EQ(test_texture->GetModifyCount(), modify_count + 1);

  // An unmodified view doesn't modify the texture, and once a view is gone the
  // texture can be changed again.
  texture_view = texture->Edit();
  ASSERT_NE(texture_view, nullptr);
  texture_view.reset();
  EXPECT_EQ(test_texture->GetModifyCount(), modify_count + 1);
  EXPECT_TRUE(texture->Clear());
  EXPECT_EQ(test_texture->GetPixel(3, 5), Pixel(0, 0, 0, 0));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
}

TEST_F(TextureTest, DeleteWhileEditing) {
  EXPECT_DEATH(
      {
        CreateSystem();
        auto texture = render_system_->CreateTexture(
            DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight);
        auto texture_view = texture->Edit();
        texture.Reset();
      },
      "ImageView is active");
}

TEST_F(TextureTest, ClearRegionWithLargeSize) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticReadWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  ASSERT_TRUE(texture->Clear());

  constexpr int kMaxInt = std::numeric_limits<int>::max();
  EXPECT_TRUE(texture->ClearRegion(5, 2, kMaxInt, kMaxInt, Pixel(1, 2, 3, 4)));
  EXPECT_THAT(
      test_texture->GetPixelRegion(5, 2, kTextureWidth - 5, kTextureHeight - 2),
      Each(Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture->GetPixelRegion(0, 0, 5, kTextureHeight),
              Each(Pixel(0, 0, 0, 0)));
  EXPECT_THAT(test_texture->GetPixelRegion(0, 0, kTextureWidth, 2),
              Each(Pixel(0, 0, 0, 0)));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
}

TEST_F(TextureTest, SetRegionWithSizeTooLargeForBuffer) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticReadWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  const int modify_count = test_texture->GetModifyCount();

  // 65536 * 65536 pixels wraps to zero in 32 bits.
  auto pixels = MakePixels(kTextureWidth * kTextureHeight, {});
  EXPECT_FALSE(texture->SetRegion(0, 0, 65536, 65536, pixels.data(),
                                  pixels.size() * sizeof(Pixel)));
  EXPECT_EQ(test_texture->GetModifyCount(), modify_count);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
}

TEST_F(TextureTest, SetRegionWithUnevenClipping) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticReadWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  ASSERT_TRUE(texture->Clear());

  // Only the bottom right 8x2 pixels of the 10x7 region are in the texture.
  auto pixels = MakePixels(10 * 7, {0, 0, 0, 255}, {1, 0, 0, 0});
  EXPECT_TRUE(texture->SetRegion(-2, -5, 10, 7, pixels));
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 8; ++x) {
      EXPECT_EQ(test_texture->GetPixel(x, y), pixels[(y + 5) * 10 + x + 2])
          << "x=" << x << ", y=" << y;
    }
  }
  EXPECT_THAT(test_texture->GetPixelRegion(8, 0, kTextureWidth - 8, 2),
              Each(Pixel(0, 0, 0, 0)));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
}

TEST_F(TextureTest, TexelSize) {
  EXPECT_EQ(GetTexelSize(TextureFormat::kRgba8Srgb), 4);
  EXPECT_EQ(GetTexelSize(TextureFormat::kRgba8), 4);
  EXPECT_EQ(GetTexelSize(TextureFormat::kRg8), 2);
  EXPECT_EQ(GetTexelSize(TextureFormat::kR8), 1);
  EXPECT_EQ(GetTexelSize(TextureFormat::kRgba4), 2);
  EXPECT_EQ(GetTexelSize(static_cast<TextureFormat>(100)), 0);
}

TEST_F(TextureTest, PixelToTexel) {
  // Only the first GetTexelSize() bytes of each texel are compared.
  const Pixel pixel(0x12, 0x34, 0x56, 0x78);
  EXPECT_EQ(PixelToTexel(TextureFormat::kRgba8Srgb, pixel), pixel.Packed());
  EXPECT_EQ(PixelToTexel(TextureFormat::kRgba8, pixel), pixel.Packed());
  EXPECT_EQ(PixelToTexel(TextureFormat::kRg8, pixel) & 0xFFFF, 0x3412u);
  EXPECT_EQ(PixelToTexel(TextureFormat::kR8, pixel) & 0xFF, 0x12u);
  EXPECT_EQ(PixelToTexel(TextureFormat::kRgba4, pixel) & 0xFFFF, 0x1357u);
}

TEST_F(TextureTest, TextureSizeInBytes) {
  EXPECT_EQ(GetTextureSizeInBytes(TextureFormat::kRgba8, 3, 5), 3 * 5 * 4);
  EXPECT_EQ(GetTextureSizeInBytes(TextureFormat::kRg8, 3, 5, 7), 3 * 5 * 7 * 2);

  // The size of the largest texture array is more than an int holds.
  EXPECT_EQ(GetTextureSizeInBytes(TextureFormat::kRgba8, kMaxTextureWidth,
                                  kMaxTextureHeight, kMaxTextureArrayCount),
            int64_t{kMaxTextureWidth} * kMaxTextureHeight *
                kMaxTextureArrayCount * 4);
}

TEST_F(TextureTest, CreateInEachFormat) {
  CreateSystem();
  for (TextureFormat format :
       {TextureFormat::kRgba8Srgb, TextureFormat::kRgba8, TextureFormat::kRg8,
        TextureFormat::kR8, TextureFormat::kRgba4}) {
    auto texture = render_system_->CreateTexture(
        DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight,
        SamplerOptions(), format);
    ASSERT_NE(texture, nullptr) << static_cast<int>(format);
    EXPECT_EQ(texture->GetFormat(), format);
    auto* test_texture = static_cast<TestTexture*>(texture.Get());
    EXPECT_EQ(test_texture->GetTexels().size(),
              kTextureWidth * kTextureHeight * GetTexelSize(format));
  }
  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(TextureTest, CreateWithInvalidFormat) {
  CreateSystem();
  EXPECT_EQ(render_system_->CreateTexture(
                DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight,
                SamplerOptions(), static_cast<TextureFormat>(100)),
            nullptr);
}

TEST_F(TextureTest, FilterOnlyFilterableFormats) {
  state_.unfilterable_formats.insert(TextureFormat::kR8);
  CreateSystem();
  EXPECT_TRUE(render_system_->CanFilter(TextureFormat::kRg8));
  EXPECT_FALSE(render_system_->CanFilter(TextureFormat::kR8));

  EXPECT_EQ(render_system_->CreateTexture(
                DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight,
                SamplerOptions().SetFilter(true), TextureFormat::kR8),
            nullptr);
  EXPECT_NE(render_system_->CreateTexture(
                DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight,
                SamplerOptions().SetFilter(false), TextureFormat::kR8),
            nullptr);
  EXPECT_NE(render_system_->CreateTexture(
                DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight,
                SamplerOptions().SetFilter(true), TextureFormat::kRg8),
            nullptr);
}

TEST_F(TextureTest, ClearConvertsToFormat) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(
      DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight,
      SamplerOptions(), TextureFormat::kRgba4);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  EXPECT_TRUE(texture->Clear(Pixel(0x12, 0x34, 0x56, 0x78)));
  const auto texels = test_texture->GetTexels();
  for (int i = 0; i < kTextureWidth * kTextureHeight; ++i) {
    ASSERT_EQ(texels[i * 2], 0x57) << i;
    ASSERT_EQ(texels[i * 2 + 1], 0x13) << i;
  }

  EXPECT_TRUE(texture->ClearRegion(1, 1, 1, 1, Pixel(0xF0, 0, 0, 0)));
  const int index = (kTextureWidth + 1) * 2;
  EXPECT_EQ(texels[index], 0x00);
  EXPECT_EQ(texels[index + 1], 0xF0);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
}

TEST_F(TextureTest, SetInTexelsOfFormat) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(
      DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight,
      SamplerOptions(), TextureFormat::kR8);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());

  std::vector<uint8_t> texels(kTextureWidth * kTextureHeight);
  for (int i = 0; i < static_cast<int>(texels.size()); ++i) {
    texels[i] = static_cast<uint8_t>(i);
  }
  EXPECT_FALSE(texture->Set(texels.data(), texels.size() - 1));
  EXPECT_TRUE(texture->Set(texels.data(), texels.size()));
  EXPECT_THAT(test_texture->GetTexels(), ElementsAreArray(texels));

  std::vector<uint8_t> read(texels.size());
  EXPECT_FALSE(texture->Get(read.data(), read.size() - 1));
  EXPECT_TRUE(texture->Get(read.data(), read.size()));
  EXPECT_THAT(read, ElementsAreArray(texels));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
}

TEST_F(TextureTest, SetRegionInTexelsOfFormat) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(
      DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight,
      SamplerOptions(), TextureFormat::kRg8);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  ASSERT_TRUE(texture->Clear());

  // A 3x2 region of two byte texels, with only the bottom right 2x1 in the
  // texture.
  const std::vector<uint8_t> texels = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  EXPECT_FALSE(
      texture->SetRegion(-1, -1, 3, 2, texels.data(), texels.size() - 1));
  EXPECT_TRUE(texture->SetRegion(-1, -1, 3, 2, texels.data(), texels.size()));
  const auto result = test_texture->GetTexels();
  EXPECT_EQ(result[0], 9);
  EXPECT_EQ(result[1], 10);
  EXPECT_EQ(result[2], 11);
  EXPECT_EQ(result[3], 12);
  EXPECT_EQ(result[4], 0);
  EXPECT_EQ(result[kTextureWidth * 2], 0);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
}

TEST_F(TextureTest, PixelsOnlyForPixelFormats) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(
      DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight,
      SamplerOptions(), TextureFormat::kRg8);
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  const int modify_count = test_texture->GetModifyCount();

  const auto pixels = MakePixels(kTextureWidth * kTextureHeight, {});
  const auto packed_pixels =
      MakePackedPixels(kTextureWidth * kTextureHeight, 0);
  EXPECT_FALSE(texture->Set(pixels));
  EXPECT_FALSE(texture->Set(packed_pixels));
  EXPECT_FALSE(texture->SetRegion(0, 0, 1, 1, pixels));
  EXPECT_FALSE(texture->SetRegion(0, 0, 1, 1, packed_pixels));
  EXPECT_EQ(texture->Edit(), nullptr);
  EXPECT_EQ(test_texture->GetModifyCount(), modify_count);

  auto rgba8_texture = render_system_->CreateTexture(
      DataVolatility::kStaticReadWrite, kTextureWidth, kTextureHeight,
      SamplerOptions(), TextureFormat::kRgba8);
  ASSERT_NE(rgba8_texture, nullptr);
  EXPECT_TRUE(rgba8_texture->Set(pixels));
  EXPECT_NE(rgba8_texture->Edit(), nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture->GetInvalidCallCount(), 0);
}

TEST_F(TextureTest, CannotGetStaticWrite) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  std::vector<Pixel> pixels(kTextureWidth * kTextureHeight);
  EXPECT_FALSE(texture->Get(pixels.data(), pixels.size() * sizeof(Pixel)));
}

TEST_F(TextureTest, CannotGetWhileEditing) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticReadWrite,
                                               kTextureWidth, kTextureHeight);
  ASSERT_NE(texture, nullptr);
  std::vector<Pixel> pixels(kTextureWidth * kTextureHeight);
  auto view = texture->Edit();
  ASSERT_NE(view, nullptr);
  EXPECT_FALSE(texture->Get(pixels.data(), pixels.size() * sizeof(Pixel)));
  view.reset();
  EXPECT_TRUE(texture->Get(pixels.data(), pixels.size() * sizeof(Pixel)));
}

}  // namespace
}  // namespace gb
