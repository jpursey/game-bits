// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/texture.h"

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
  EXPECT_FALSE(texture->Set(pixels.data(), pixels.size()));
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

}  // namespace
}  // namespace gb
