// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/texture_view.h"

#include "gb/render/render_test.h"
#include "gb/render/test_texture.h"
#include "gb/render/texture.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Each;
using ::testing::ElementsAreArray;

constexpr int kTextureWidth = 16;
constexpr int kTextureHeight = 32;

class TextureViewTest : public RenderTest {
 protected:
  void SetUp() override {
    CreateSystem();
    texture_ = render_system_->CreateTexture(&temp_resource_set_,
                                             DataVolatility::kPerFrame,
                                             kTextureWidth, kTextureHeight);
    ASSERT_NE(texture_, nullptr);
    test_texture_ = static_cast<TestTexture*>(texture_);
  }

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

  Texture* texture_ = nullptr;
  TestTexture* test_texture_ = nullptr;
};

TEST_F(TextureViewTest, Properties) {
  auto view = texture_->Edit();
  ASSERT_NE(view, nullptr);
  const TextureView* const_view = view.get();

  EXPECT_EQ(const_view->GetWidth(), kTextureWidth);
  EXPECT_EQ(const_view->GetHeight(), kTextureHeight);
  EXPECT_EQ(const_view->GetCount(), kTextureWidth * kTextureHeight);
  EXPECT_EQ(const_view->GetSizeInBytes(),
            kTextureWidth * kTextureHeight * sizeof(Pixel));
  EXPECT_EQ(const_view->GetPixels(), test_texture_->GetPixelData());
  EXPECT_EQ(const_view->GetPackedPixels(),
            static_cast<void*>(test_texture_->GetPixelData()));
  EXPECT_EQ(const_view->GetRawPixels(),
            static_cast<void*>(test_texture_->GetPixelData()));
  EXPECT_FALSE(const_view->IsModified());

  view.reset();
  EXPECT_EQ(test_texture_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_->GetModifyCount(), 0);
}

TEST_F(TextureViewTest, ModifyPixels) {
  auto view = texture_->Edit();
  ASSERT_NE(view, nullptr);

  EXPECT_EQ(view->ModifyPixels(), test_texture_->GetPixelData());
  EXPECT_EQ(view->ModifyPackedPixels(),
            static_cast<void*>(test_texture_->GetPixelData()));
  EXPECT_EQ(view->ModifyRawPixels(),
            static_cast<void*>(test_texture_->GetPixelData()));
  EXPECT_TRUE(view->IsModified());

  view.reset();
  EXPECT_EQ(test_texture_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_->GetModifyCount(), 1);
}

TEST_F(TextureViewTest, IndividualPixelAccess) {
  texture_->Set(
      MakePixels(kTextureWidth * kTextureHeight, {0, 0, 0, 0}, {1, 2, 3, 4}));
  auto view = texture_->Edit();
  ASSERT_NE(view, nullptr);
  const TextureView* const_view = view.get();

  for (int x = 0; x < kTextureWidth; ++x) {
    for (int y = 0; y < kTextureHeight; ++y) {
      EXPECT_EQ(const_view->Get(x, y), test_texture_->GetPixel(x, y));
    }
  }
  EXPECT_FALSE(view->IsModified());

  for (int x = 0; x < kTextureWidth; ++x) {
    for (int y = 0; y < kTextureHeight; ++y) {
      Pixel value = {static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                     static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
      view->Modify(x, y) = value;
      EXPECT_EQ(test_texture_->GetPixel(x, y), value);
    }
  }
  EXPECT_TRUE(view->IsModified());

  view.reset();
  EXPECT_EQ(test_texture_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_->GetModifyCount(), 2);
}

TEST_F(TextureViewTest, ConstRegionProperties) {
  auto view = texture_->Edit();
  ASSERT_NE(view, nullptr);
  const TextureView* const_view = view.get();

  auto const_region = const_view->GetRegion();
  EXPECT_EQ(const_region.GetX(), 0);
  EXPECT_EQ(const_region.GetY(), 0);
  EXPECT_EQ(const_region.GetWidth(), kTextureWidth);
  EXPECT_EQ(const_region.GetHeight(), kTextureHeight);

  auto const_sub_region = const_view->GetRegion(4, 5, 6, 7);
  EXPECT_EQ(const_sub_region.GetX(), 4);
  EXPECT_EQ(const_sub_region.GetY(), 5);
  EXPECT_EQ(const_sub_region.GetWidth(), 6);
  EXPECT_EQ(const_sub_region.GetHeight(), 7);

  EXPECT_FALSE(view->IsModified());
  view.reset();
  EXPECT_EQ(test_texture_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_->GetModifyCount(), 0);
}

TEST_F(TextureViewTest, ConstRegionGetPixel) {
  texture_->Set(
      MakePixels(kTextureWidth * kTextureHeight, {0, 0, 0, 0}, {1, 2, 3, 4}));
  auto view = texture_->Edit();
  ASSERT_NE(view, nullptr);
  const TextureView* const_view = view.get();

  auto const_region = const_view->GetRegion();
  for (int x = 0; x < const_region.GetWidth(); ++x) {
    for (int y = 0; y < const_region.GetHeight(); ++y) {
      EXPECT_EQ(const_region.Get(x, y), test_texture_->GetPixel(x, y));
    }
  }

  auto const_sub_region = const_view->GetRegion(4, 5, 6, 7);
  for (int x = 0; x < const_sub_region.GetWidth(); ++x) {
    for (int y = 0; y < const_sub_region.GetHeight(); ++y) {
      EXPECT_EQ(const_sub_region.Get(x, y),
                test_texture_->GetPixel(x + 4, y + 5));
    }
  }

  EXPECT_FALSE(view->IsModified());
  view.reset();
  EXPECT_EQ(test_texture_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_->GetModifyCount(), 1);
}

TEST_F(TextureViewTest, ConstRegionGetAll) {
  texture_->Set(
      MakePixels(kTextureWidth * kTextureHeight, {0, 0, 0, 0}, {1, 2, 3, 4}));
  auto view = texture_->Edit();
  ASSERT_NE(view, nullptr);
  const TextureView* const_view = view.get();

  std::vector<Pixel> pixels;
  std::vector<uint32_t> packed_pixels;

  auto const_region = const_view->GetRegion();
  const_region.GetAll(&pixels);
  EXPECT_THAT(pixels, ElementsAreArray(test_texture_->GetPixels()));
  const_region.GetAll(&packed_pixels);
  EXPECT_THAT(packed_pixels,
              ElementsAreArray(test_texture_->GetPackedPixels()));
  pixels.clear();
  pixels.resize(const_region.GetWidth() * const_region.GetHeight());
  const_region.GetAll(pixels.data(), pixels.size() * sizeof(Pixel));
  EXPECT_THAT(pixels, ElementsAreArray(test_texture_->GetPixels()));
  pixels.clear();
  pixels.resize(const_region.GetWidth() * const_region.GetHeight());
  const_region.GetAll(pixels.data(), pixels.size() / 2 * sizeof(Pixel));
  EXPECT_THAT(absl::MakeConstSpan(pixels.data(), pixels.size() / 2),
              ElementsAreArray(test_texture_->GetPixelRegion(
                  0, 0, kTextureWidth, kTextureHeight / 2)));
  EXPECT_THAT(
      absl::MakeConstSpan(pixels.data() + pixels.size() / 2, pixels.size() / 2),
      Each(Pixel()));

  auto const_sub_region = const_view->GetRegion(4, 5, 6, 7);
  const_sub_region.GetAll(&pixels);
  EXPECT_THAT(pixels,
              ElementsAreArray(test_texture_->GetPixelRegion(4, 5, 6, 7)));
  const_sub_region.GetAll(&packed_pixels);
  EXPECT_THAT(
      packed_pixels,
      ElementsAreArray(test_texture_->GetPackedPixelRegion(4, 5, 6, 7)));
  pixels.clear();
  pixels.resize(const_sub_region.GetWidth() * const_sub_region.GetHeight());
  const_sub_region.GetAll(pixels.data(), pixels.size() * sizeof(Pixel));
  EXPECT_THAT(pixels,
              ElementsAreArray(test_texture_->GetPixelRegion(4, 5, 6, 7)));
  pixels.clear();
  pixels.resize(const_sub_region.GetWidth() * const_sub_region.GetHeight());
  const_sub_region.GetAll(pixels.data(), pixels.size() / 2 * sizeof(Pixel));
  auto test_pixels = test_texture_->GetPixelRegion(4, 5, 6, 7);
  test_pixels.resize(test_pixels.size() / 2);
  EXPECT_THAT(absl::MakeConstSpan(pixels.data(), pixels.size() / 2),
              ElementsAreArray(test_pixels));
  EXPECT_THAT(
      absl::MakeConstSpan(pixels.data() + pixels.size() / 2, pixels.size() / 2),
      Each(Pixel()));

  EXPECT_FALSE(view->IsModified());
  view.reset();
  EXPECT_EQ(test_texture_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_->GetModifyCount(), 1);
}

TEST_F(TextureViewTest, RegionModifyPixel) {
  auto view = texture_->Edit();
  ASSERT_NE(view, nullptr);

  auto region = view->ModifyRegion();
  for (int x = 0; x < region.GetWidth(); ++x) {
    for (int y = 0; y < region.GetHeight(); ++y) {
      Pixel value = {static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                     static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
      region.Modify(x, y) = value;
      EXPECT_EQ(test_texture_->GetPixel(x, y), value);
    }
  }

  auto sub_region = view->ModifyRegion(4, 5, 6, 7);
  for (int x = 0; x < sub_region.GetWidth(); ++x) {
    for (int y = 0; y < sub_region.GetHeight(); ++y) {
      Pixel value = {static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                     static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
      sub_region.Modify(x, y) = value;
      EXPECT_EQ(test_texture_->GetPixel(x + 4, y + 5), value);
    }
  }

  EXPECT_TRUE(view->IsModified());
  view.reset();
  EXPECT_EQ(test_texture_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_->GetModifyCount(), 1);
}

TEST_F(TextureViewTest, RegionSetAll) {
  auto view = texture_->Edit();
  ASSERT_NE(view, nullptr);

  std::vector<Pixel> pixels =
      MakePixels(kTextureWidth * kTextureHeight, {0, 0, 0, 0}, {1, 2, 3, 4});
  std::vector<uint32_t> packed_pixels =
      MakePackedPixels(kTextureWidth * kTextureHeight, 0, 1);

  auto region = view->ModifyRegion();
  region.SetAll(pixels);
  EXPECT_THAT(test_texture_->GetPixels(), ElementsAreArray(pixels));
  region.SetAll(packed_pixels);
  EXPECT_THAT(test_texture_->GetPackedPixels(),
              ElementsAreArray(packed_pixels));
  region.SetAll(pixels.data(), pixels.size() * sizeof(Pixel));
  EXPECT_THAT(test_texture_->GetPixels(), ElementsAreArray(pixels));
  region.SetAll(packed_pixels.data(),
                packed_pixels.size() / 2 * sizeof(uint32_t));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(0, 0, kTextureWidth,
                                                  kTextureHeight / 2),
              ElementsAreArray(absl::MakeConstSpan(packed_pixels.data(),
                                                   packed_pixels.size() / 2)));
  EXPECT_THAT(test_texture_->GetPixelRegion(0, kTextureHeight / 2,
                                            kTextureWidth, kTextureHeight / 2),
              ElementsAreArray(absl::MakeConstSpan(
                  pixels.data() + pixels.size() / 2, pixels.size() / 2)));

  pixels = MakePixels(6 * 7, {0, 0, 0, 0}, {1, 2, 3, 4});
  packed_pixels = MakePackedPixels(6 * 7, 0, 1);
  auto sub_region = view->ModifyRegion(4, 5, 6, 7);
  sub_region.SetAll(pixels);
  EXPECT_THAT(test_texture_->GetPixelRegion(4, 5, 6, 7),
              ElementsAreArray(pixels));
  sub_region.SetAll(packed_pixels);
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(4, 5, 6, 7),
              ElementsAreArray(packed_pixels));
  sub_region.SetAll(pixels.data(), pixels.size() * sizeof(Pixel));
  EXPECT_THAT(test_texture_->GetPixelRegion(4, 5, 6, 7),
              ElementsAreArray(pixels));
  sub_region.SetAll(packed_pixels.data(),
                    packed_pixels.size() / 2 * sizeof(uint32_t));
  auto test_pixels = test_texture_->GetPixelRegion(4, 5, 6, 7);
  auto test_packed_pixels = test_texture_->GetPackedPixelRegion(4, 5, 6, 7);
  EXPECT_THAT(absl::MakeConstSpan(test_packed_pixels.data(),
                                  test_packed_pixels.size() / 2),
              ElementsAreArray(absl::MakeConstSpan(packed_pixels.data(),
                                                   packed_pixels.size() / 2)));
  EXPECT_THAT(absl::MakeConstSpan(test_pixels.data() + test_pixels.size() / 2,
                                  test_pixels.size() / 2),
              ElementsAreArray(absl::MakeConstSpan(
                  pixels.data() + pixels.size() / 2, pixels.size() / 2)));

  EXPECT_TRUE(view->IsModified());
  view.reset();
  EXPECT_EQ(test_texture_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_->GetModifyCount(), 1);
}

TEST_F(TextureViewTest, RegionClear) {
  texture_->Set(
      MakePixels(kTextureWidth * kTextureHeight, {0, 0, 0, 0}, {1, 2, 3, 4}));
  auto view = texture_->Edit();
  ASSERT_NE(view, nullptr);

  auto region = view->ModifyRegion();
  region.Clear();
  EXPECT_THAT(test_texture_->GetPixels(), Each(Pixel(0, 0, 0, 0)));
  region.Clear(Pixel(1, 2, 3, 4));
  EXPECT_THAT(test_texture_->GetPixels(), Each(Pixel(1, 2, 3, 4)));
  region.Clear(0xdeadbeef);
  EXPECT_THAT(test_texture_->GetPackedPixels(), Each(0xdeadbeef));
  region.Clear(0xFFFFFFFF);

  auto sub_region = view->ModifyRegion(4, 5, 6, 7);
  sub_region.Clear();
  EXPECT_THAT(test_texture_->GetPixelRegion(4, 5, 6, 7),
              Each(Pixel(0, 0, 0, 0)));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(0, 0, kTextureWidth, 5),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(0, 12, kTextureWidth,
                                                  kTextureHeight - 12),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(0, 5, 4, 7),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(10, 5, kTextureWidth - 10, 7),
              Each(0xFFFFFFFF));
  sub_region.Clear(Pixel(1, 2, 3, 4));
  EXPECT_THAT(test_texture_->GetPixelRegion(4, 5, 6, 7),
              Each(Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(0, 0, kTextureWidth, 5),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(0, 12, kTextureWidth,
                                                  kTextureHeight - 12),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(0, 5, 4, 7),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(10, 5, kTextureWidth - 10, 7),
              Each(0xFFFFFFFF));
  sub_region.Clear(0xdeadbeef);
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(4, 5, 6, 7),
              Each(0xdeadbeef));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(0, 0, kTextureWidth, 5),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(0, 12, kTextureWidth,
                                                  kTextureHeight - 12),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(0, 5, 4, 7),
              Each(0xFFFFFFFF));
  EXPECT_THAT(test_texture_->GetPackedPixelRegion(10, 5, kTextureWidth - 10, 7),
              Each(0xFFFFFFFF));

  EXPECT_TRUE(view->IsModified());
  view.reset();
  EXPECT_EQ(test_texture_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_->GetModifyCount(), 2);
}

}  // namespace
}  // namespace gb
