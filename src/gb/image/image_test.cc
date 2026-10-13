// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/image/image.h"

#include "gb/base/allocator.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(ImageTest, Properties) {
  Image image(4, 3);
  EXPECT_EQ(image.GetWidth(), 4);
  EXPECT_EQ(image.GetHeight(), 3);
  EXPECT_EQ(image.GetCount(), 12);
  EXPECT_EQ(image.GetSizeInBytes(), 12 * static_cast<int>(sizeof(Pixel)));
  EXPECT_THAT(image.GetPixels(), SizeIs(12));
  EXPECT_THAT(image.GetPackedPixels(), SizeIs(12));
  EXPECT_EQ(image.GetRawPixels(), image.GetPixels().data());
}

TEST(ImageTest, EmptyImage) {
  Image image(0, 0);
  EXPECT_EQ(image.GetCount(), 0);
  EXPECT_THAT(image.GetPixels(), IsEmpty());
}

TEST(ImageTest, ConstructWithPixel) {
  Image image(4, 3, Pixel(1, 2, 3, 4));
  EXPECT_THAT(image.GetPixels(), Each(Pixel(1, 2, 3, 4)));
}

TEST(ImageTest, ConstructWithPixels) {
  Pixel* const pixels = new Pixel[2]{Pixel(1, 2, 3, 4), Pixel(5, 6, 7, 8)};
  int free_count = 0;
  void* freed_pixels = nullptr;
  {
    Image image(2, 1, pixels, [&](void* image_pixels) {
      ++free_count;
      freed_pixels = image_pixels;
      delete[] static_cast<Pixel*>(image_pixels);
    });
    EXPECT_EQ(image.GetRawPixels(), pixels);
    EXPECT_THAT(image.GetPixels(),
                ElementsAre(Pixel(1, 2, 3, 4), Pixel(5, 6, 7, 8)));
    EXPECT_EQ(free_count, 0);
  }
  EXPECT_EQ(free_count, 1);
  EXPECT_EQ(freed_pixels, pixels);
}

TEST(ImageTest, ConstructWithPixelsFromDefaultAllocator) {
  void* pixels =
      GetDefaultAllocator()->Alloc(2 * sizeof(Pixel), alignof(Pixel));
  Image image(2, 1, pixels, nullptr);
  EXPECT_EQ(image.GetRawPixels(), pixels);
}

TEST(ImageTest, Edit) {
  Image image(4, 3, Pixel(1, 2, 3, 4));
  {
    ImageView view = image.Edit();
    EXPECT_EQ(view.GetWidth(), 4);
    EXPECT_EQ(view.GetHeight(), 3);
    view.Modify(1, 2) = Pixel(5, 6, 7, 8);
  }
  EXPECT_EQ(image.GetPixels()[2 * 4 + 1], Pixel(5, 6, 7, 8));
}

TEST(ImageTest, View) {
  Image image(4, 3, Pixel(1, 2, 3, 4));
  image.Edit().Modify(1, 2) = Pixel(5, 6, 7, 8);

  const Image& const_image = image;
  const ImageView& view = const_image.View();
  EXPECT_EQ(view.GetWidth(), 4);
  EXPECT_EQ(view.GetHeight(), 3);
  EXPECT_EQ(view.GetRawPixels(), image.GetRawPixels());
  EXPECT_EQ(view.Get(1, 2), Pixel(5, 6, 7, 8));
}

}  // namespace
}  // namespace gb
