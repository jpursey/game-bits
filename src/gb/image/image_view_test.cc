// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/image/image_view.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Each;
using ::testing::ElementsAreArray;

constexpr int kImageWidth = 16;
constexpr int kImageHeight = 32;

struct TestImage final {
 public:
  TestImage() : pixels_(kImageWidth * kImageHeight, PixelColor::kWhite) {}

  Pixel GetPixel(int x, int y) const { return pixels_[y * kImageWidth + x]; }
  absl::Span<const Pixel> GetPixels() const {
    return absl::MakeConstSpan(pixels_);
  }
  absl::Span<const uint32_t> GetPackedPixels() const {
    return absl::MakeConstSpan(
        reinterpret_cast<const uint32_t*>(pixels_.data()),
        kImageWidth * kImageHeight);
  }
  void* GetRawPixels() { return pixels_.data(); }
  std::vector<Pixel> GetPixelRegion(int x, int y, int width, int height) const {
    std::vector<Pixel> pixels(width * height);
    for (int j = 0; j < height; ++j) {
      for (int i = 0; i < width; ++i) {
        pixels[j * width + i] = pixels_[(y + j) * kImageWidth + (x + i)];
      }
    }
    return pixels;
  }
  std::vector<uint32_t> GetPackedPixelRegion(int x, int y, int width,
                                             int height) const {
    std::vector<uint32_t> pixels(width * height);
    for (int j = 0; j < height; ++j) {
      for (int i = 0; i < width; ++i) {
        pixels[j * width + i] =
            pixels_[(y + j) * kImageWidth + (x + i)].Packed();
      }
    }
    return pixels;
  }

  std::vector<Pixel> pixels_;
};

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

TEST(ImageViewTest, Properties) {
  TestImage image;
  const ImageView const_view(kImageWidth, kImageHeight, image.GetRawPixels());
  EXPECT_EQ(const_view.GetWidth(), kImageWidth);
  EXPECT_EQ(const_view.GetHeight(), kImageHeight);
  EXPECT_EQ(const_view.GetCount(), kImageWidth * kImageHeight);
  EXPECT_EQ(const_view.GetSizeInBytes(),
            kImageWidth * kImageHeight * sizeof(Pixel));
  EXPECT_EQ(const_view.GetPixels(), image.GetRawPixels());
  EXPECT_EQ(const_view.GetPackedPixels(), image.GetRawPixels());
  EXPECT_EQ(const_view.GetRawPixels(), image.GetRawPixels());
  EXPECT_FALSE(const_view.IsModified());
}

TEST(ImageViewTest, IndividualPixelAccess) {
  TestImage image;
  image.pixels_ =
      MakePixels(kImageWidth * kImageHeight, {0, 0, 0, 0}, {1, 2, 3, 4});

  const ImageView const_view(kImageWidth, kImageHeight, image.GetRawPixels());
  for (int x = 0; x < kImageWidth; ++x) {
    for (int y = 0; y < kImageHeight; ++y) {
      EXPECT_EQ(const_view.Get(x, y), image.GetPixel(x, y));
    }
  }
  EXPECT_FALSE(const_view.IsModified());

  ImageView view(kImageWidth, kImageHeight, image.GetRawPixels());
  for (int x = 0; x < kImageWidth; ++x) {
    for (int y = 0; y < kImageHeight; ++y) {
      Pixel value = {static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                     static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
      view.Modify(x, y) = value;
      EXPECT_EQ(value, image.GetPixel(x, y));
    }
  }
  EXPECT_TRUE(view.IsModified());
  EXPECT_FALSE(const_view.IsModified());
}

TEST(ImageViewTest, ConstRegionProperties) {
  TestImage image;
  const ImageView const_view(kImageWidth, kImageHeight, image.GetRawPixels());

  auto const_region = const_view.GetRegion();
  EXPECT_EQ(const_region.GetX(), 0);
  EXPECT_EQ(const_region.GetY(), 0);
  EXPECT_EQ(const_region.GetWidth(), kImageWidth);
  EXPECT_EQ(const_region.GetHeight(), kImageHeight);

  auto const_sub_region = const_view.GetRegion(4, 5, 6, 7);
  EXPECT_EQ(const_sub_region.GetX(), 4);
  EXPECT_EQ(const_sub_region.GetY(), 5);
  EXPECT_EQ(const_sub_region.GetWidth(), 6);
  EXPECT_EQ(const_sub_region.GetHeight(), 7);

  EXPECT_FALSE(const_view.IsModified());
}

TEST(ImageViewTest, ConstRegionGetPixel) {
  TestImage image;
  image.pixels_ =
      MakePixels(kImageWidth * kImageHeight, {0, 0, 0, 0}, {1, 2, 3, 4});
  const ImageView const_view(kImageWidth, kImageHeight, image.GetRawPixels());

  auto const_region = const_view.GetRegion();
  for (int x = 0; x < const_region.GetWidth(); ++x) {
    for (int y = 0; y < const_region.GetHeight(); ++y) {
      EXPECT_EQ(const_region.Get(x, y), image.GetPixel(x, y));
    }
  }

  auto const_sub_region = const_view.GetRegion(4, 5, 6, 7);
  for (int x = 0; x < const_sub_region.GetWidth(); ++x) {
    for (int y = 0; y < const_sub_region.GetHeight(); ++y) {
      EXPECT_EQ(const_sub_region.Get(x, y), image.GetPixel(x + 4, y + 5));
    }
  }

  EXPECT_FALSE(const_view.IsModified());
}

TEST(ImageViewTest, ConstRegionGetAll) {
  TestImage image;
  image.pixels_ =
      MakePixels(kImageWidth * kImageHeight, {0, 0, 0, 0}, {1, 2, 3, 4});
  const ImageView const_view(kImageWidth, kImageHeight, image.GetRawPixels());

  std::vector<Pixel> pixels;
  std::vector<uint32_t> packed_pixels;

  auto const_region = const_view.GetRegion();
  const_region.GetAll(&pixels);
  EXPECT_THAT(pixels, ElementsAreArray(image.GetPixels()));
  const_region.GetAll(&packed_pixels);
  EXPECT_THAT(packed_pixels, ElementsAreArray(image.GetPackedPixels()));
  pixels.clear();
  pixels.resize(const_region.GetWidth() * const_region.GetHeight());
  const_region.GetAll(pixels.data(), pixels.size() * sizeof(Pixel));
  EXPECT_THAT(pixels, ElementsAreArray(image.GetPixels()));
  pixels.clear();
  pixels.resize(const_region.GetWidth() * const_region.GetHeight());
  const_region.GetAll(pixels.data(), pixels.size() / 2 * sizeof(Pixel));
  EXPECT_THAT(absl::MakeConstSpan(pixels.data(), pixels.size() / 2),
              ElementsAreArray(
                  image.GetPixelRegion(0, 0, kImageWidth, kImageHeight / 2)));
  EXPECT_THAT(
      absl::MakeConstSpan(pixels.data() + pixels.size() / 2, pixels.size() / 2),
      Each(Pixel()));

  auto const_sub_region = const_view.GetRegion(4, 5, 6, 7);
  const_sub_region.GetAll(&pixels);
  EXPECT_THAT(pixels, ElementsAreArray(image.GetPixelRegion(4, 5, 6, 7)));
  const_sub_region.GetAll(&packed_pixels);
  EXPECT_THAT(packed_pixels,
              ElementsAreArray(image.GetPackedPixelRegion(4, 5, 6, 7)));
  pixels.clear();
  pixels.resize(const_sub_region.GetWidth() * const_sub_region.GetHeight());
  const_sub_region.GetAll(pixels.data(), pixels.size() * sizeof(Pixel));
  EXPECT_THAT(pixels, ElementsAreArray(image.GetPixelRegion(4, 5, 6, 7)));
  pixels.clear();
  pixels.resize(const_sub_region.GetWidth() * const_sub_region.GetHeight());
  const_sub_region.GetAll(pixels.data(), pixels.size() / 2 * sizeof(Pixel));
  auto test_pixels = image.GetPixelRegion(4, 5, 6, 7);
  test_pixels.resize(test_pixels.size() / 2);
  EXPECT_THAT(absl::MakeConstSpan(pixels.data(), pixels.size() / 2),
              ElementsAreArray(test_pixels));
  EXPECT_THAT(
      absl::MakeConstSpan(pixels.data() + pixels.size() / 2, pixels.size() / 2),
      Each(Pixel()));

  EXPECT_FALSE(const_view.IsModified());
}

TEST(ImageViewTest, RegionModifyPixel) {
  TestImage image;
  ImageView view(kImageWidth, kImageHeight, image.GetRawPixels());

  auto region = view.ModifyRegion();
  for (int x = 0; x < region.GetWidth(); ++x) {
    for (int y = 0; y < region.GetHeight(); ++y) {
      Pixel value = {static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                     static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
      region.Modify(x, y) = value;
      EXPECT_EQ(image.GetPixel(x, y), value);
    }
  }

  auto sub_region = view.ModifyRegion(4, 5, 6, 7);
  for (int x = 0; x < sub_region.GetWidth(); ++x) {
    for (int y = 0; y < sub_region.GetHeight(); ++y) {
      Pixel value = {static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                     static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
      sub_region.Modify(x, y) = value;
      EXPECT_EQ(image.GetPixel(x + 4, y + 5), value);
    }
  }

  EXPECT_TRUE(view.IsModified());
}

TEST(ImageViewTest, RegionSetAll) {
  TestImage image;
  ImageView view(kImageWidth, kImageHeight, image.GetRawPixels());

  std::vector<Pixel> pixels =
      MakePixels(kImageWidth * kImageHeight, {0, 0, 0, 0}, {1, 2, 3, 4});
  std::vector<uint32_t> packed_pixels =
      MakePackedPixels(kImageWidth * kImageHeight, 0, 1);

  auto region = view.ModifyRegion();
  region.SetAll(pixels);
  EXPECT_THAT(image.GetPixels(), ElementsAreArray(pixels));
  region.SetAll(packed_pixels);
  EXPECT_THAT(image.GetPackedPixels(), ElementsAreArray(packed_pixels));
  region.SetAll(pixels.data(), pixels.size() * sizeof(Pixel));
  EXPECT_THAT(image.GetPixels(), ElementsAreArray(pixels));
  region.SetAll(packed_pixels.data(),
                packed_pixels.size() / 2 * sizeof(uint32_t));
  EXPECT_THAT(image.GetPackedPixelRegion(0, 0, kImageWidth, kImageHeight / 2),
              ElementsAreArray(absl::MakeConstSpan(packed_pixels.data(),
                                                   packed_pixels.size() / 2)));
  EXPECT_THAT(
      image.GetPixelRegion(0, kImageHeight / 2, kImageWidth, kImageHeight / 2),
      ElementsAreArray(absl::MakeConstSpan(pixels.data() + pixels.size() / 2,
                                           pixels.size() / 2)));

  pixels = MakePixels(6 * 7, {0, 0, 0, 0}, {1, 2, 3, 4});
  packed_pixels = MakePackedPixels(6 * 7, 0, 1);
  auto sub_region = view.ModifyRegion(4, 5, 6, 7);
  sub_region.SetAll(pixels);
  EXPECT_THAT(image.GetPixelRegion(4, 5, 6, 7), ElementsAreArray(pixels));
  sub_region.SetAll(packed_pixels);
  EXPECT_THAT(image.GetPackedPixelRegion(4, 5, 6, 7),
              ElementsAreArray(packed_pixels));
  sub_region.SetAll(pixels.data(), pixels.size() * sizeof(Pixel));
  EXPECT_THAT(image.GetPixelRegion(4, 5, 6, 7), ElementsAreArray(pixels));
  sub_region.SetAll(packed_pixels.data(),
                    packed_pixels.size() / 2 * sizeof(uint32_t));
  auto test_pixels = image.GetPixelRegion(4, 5, 6, 7);
  auto test_packed_pixels = image.GetPackedPixelRegion(4, 5, 6, 7);
  EXPECT_THAT(absl::MakeConstSpan(test_packed_pixels.data(),
                                  test_packed_pixels.size() / 2),
              ElementsAreArray(absl::MakeConstSpan(packed_pixels.data(),
                                                   packed_pixels.size() / 2)));
  EXPECT_THAT(absl::MakeConstSpan(test_pixels.data() + test_pixels.size() / 2,
                                  test_pixels.size() / 2),
              ElementsAreArray(absl::MakeConstSpan(
                  pixels.data() + pixels.size() / 2, pixels.size() / 2)));

  EXPECT_TRUE(view.IsModified());
}

TEST(ImageViewTest, RegionClear) {
  TestImage image;
  image.pixels_ =
      MakePixels(kImageWidth * kImageHeight, {0, 0, 0, 0}, {1, 2, 3, 4});
  ImageView view(kImageWidth, kImageHeight, image.GetRawPixels());

  auto region = view.ModifyRegion();
  region.Clear();
  EXPECT_THAT(image.GetPixels(), Each(Pixel(0, 0, 0, 0)));
  region.Clear(Pixel(1, 2, 3, 4));
  EXPECT_THAT(image.GetPixels(), Each(Pixel(1, 2, 3, 4)));
  region.Clear(0xdeadbeef);
  EXPECT_THAT(image.GetPackedPixels(), Each(0xdeadbeef));
  region.Clear(0xFFFFFFFF);

  auto sub_region = view.ModifyRegion(4, 5, 6, 7);
  sub_region.Clear();
  EXPECT_THAT(image.GetPixelRegion(4, 5, 6, 7), Each(Pixel(0, 0, 0, 0)));
  EXPECT_THAT(image.GetPackedPixelRegion(0, 0, kImageWidth, 5),
              Each(0xFFFFFFFF));
  EXPECT_THAT(image.GetPackedPixelRegion(0, 12, kImageWidth, kImageHeight - 12),
              Each(0xFFFFFFFF));
  EXPECT_THAT(image.GetPackedPixelRegion(0, 5, 4, 7), Each(0xFFFFFFFF));
  EXPECT_THAT(image.GetPackedPixelRegion(10, 5, kImageWidth - 10, 7),
              Each(0xFFFFFFFF));
  sub_region.Clear(Pixel(1, 2, 3, 4));
  EXPECT_THAT(image.GetPixelRegion(4, 5, 6, 7), Each(Pixel(1, 2, 3, 4)));
  EXPECT_THAT(image.GetPackedPixelRegion(0, 0, kImageWidth, 5),
              Each(0xFFFFFFFF));
  EXPECT_THAT(image.GetPackedPixelRegion(0, 12, kImageWidth, kImageHeight - 12),
              Each(0xFFFFFFFF));
  EXPECT_THAT(image.GetPackedPixelRegion(0, 5, 4, 7), Each(0xFFFFFFFF));
  EXPECT_THAT(image.GetPackedPixelRegion(10, 5, kImageWidth - 10, 7),
              Each(0xFFFFFFFF));
  sub_region.Clear(0xdeadbeef);
  EXPECT_THAT(image.GetPackedPixelRegion(4, 5, 6, 7), Each(0xdeadbeef));
  EXPECT_THAT(image.GetPackedPixelRegion(0, 0, kImageWidth, 5),
              Each(0xFFFFFFFF));
  EXPECT_THAT(image.GetPackedPixelRegion(0, 12, kImageWidth, kImageHeight - 12),
              Each(0xFFFFFFFF));
  EXPECT_THAT(image.GetPackedPixelRegion(0, 5, 4, 7), Each(0xFFFFFFFF));
  EXPECT_THAT(image.GetPackedPixelRegion(10, 5, kImageWidth - 10, 7),
              Each(0xFFFFFFFF));

  EXPECT_TRUE(view.IsModified());
}

}  // namespace
}  // namespace gb
