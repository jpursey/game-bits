// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/texture_array.h"

#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Each;
using ::testing::ElementsAreArray;

constexpr int kTextureCount = 2;
constexpr int kTextureWidth = 16;
constexpr int kTextureHeight = 32;

class TextureArrayTest : public RenderTest {
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

TEST_F(TextureArrayTest, CreateAsResourcePtr) {
  CreateSystem();
  ResourcePtr<TextureArray> texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticWrite, kTextureCount, kTextureWidth,
      kTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture_array->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_array->GetModifyCount(), 0);
}

TEST_F(TextureArrayTest, CreateInResourceSet) {
  CreateSystem();
  ResourceSet resource_set;
  TextureArray* texture_array = render_system_->CreateTextureArray(
      &resource_set, DataVolatility::kStaticWrite, kTextureCount, kTextureWidth,
      kTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array = static_cast<TestTextureArray*>(texture_array);
  EXPECT_EQ(resource_set.Get<TextureArray>(texture_array->GetResourceId()),
            texture_array);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture_array->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_array->GetModifyCount(), 0);
}

TEST_F(TextureArrayTest, Properties) {
  CreateSystem();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticWrite, kTextureCount, kTextureWidth,
      kTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());

  EXPECT_EQ(texture_array->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(texture_array->GetCount(), kTextureCount);
  EXPECT_EQ(texture_array->GetWidth(), kTextureWidth);
  EXPECT_EQ(texture_array->GetHeight(), kTextureHeight);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture_array->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_array->GetModifyCount(), 0);
}

TEST_F(TextureArrayTest, Clear) {
  CreateSystem();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticWrite, kTextureCount, kTextureWidth,
      kTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());

  EXPECT_TRUE(texture_array->Clear(0));
  EXPECT_THAT(test_texture_array->GetPixels(0), Each(Pixel(0, 0, 0, 0)));
  EXPECT_THAT(test_texture_array->GetPackedPixels(1), Each(0xFFFFFFFF));

  EXPECT_TRUE(texture_array->Clear(1));
  EXPECT_THAT(test_texture_array->GetPixels(1), Each(Pixel(0, 0, 0, 0)));

  EXPECT_TRUE(texture_array->Clear(0, Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture_array->GetPixels(0), Each(Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture_array->GetPixels(1), Each(Pixel(0, 0, 0, 0)));

  EXPECT_TRUE(texture_array->Clear(1, 0xdeadbeef));
  EXPECT_THAT(test_texture_array->GetPixels(0), Each(Pixel(1, 2, 3, 4)));
  EXPECT_THAT(test_texture_array->GetPackedPixels(1), Each(0xdeadbeef));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture_array->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_array->GetModifyCount(), 4);
}

TEST_F(TextureArrayTest, FailClear) {
  CreateSystem();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticWrite, 1, kTextureWidth, kTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());

  state_.texture_array_config.fail_clear = true;
  EXPECT_FALSE(texture_array->Clear(0));
  EXPECT_FALSE(texture_array->Clear(0, Pixel(1, 2, 3, 4)));
  EXPECT_FALSE(texture_array->Clear(0, 0xdeadbeef));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture_array->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_array->GetModifyCount(), 0);
}

TEST_F(TextureArrayTest, Set) {
  CreateSystem();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticWrite, kTextureCount, kTextureWidth,
      kTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());

  std::vector<Pixel> pixels[] = {
      MakePixels(kTextureWidth * kTextureHeight, {0, 0, 0, 0}, {1, 2, 3, 4}),
      MakePixels(kTextureWidth * kTextureHeight, {0, 0, 0, 0}, {4, 3, 2, 1}),
  };
  std::vector<uint32_t> packed_pixels[] = {
      MakePackedPixels(kTextureWidth * kTextureHeight, 0, 1),
      MakePackedPixels(kTextureWidth * kTextureHeight, 0, 2),
  };

  EXPECT_TRUE(texture_array->Set(0, pixels[0]));
  EXPECT_THAT(test_texture_array->GetPixels(0), ElementsAreArray(pixels[0]));
  EXPECT_THAT(test_texture_array->GetPackedPixels(1), Each(0xFFFFFFFF));

  EXPECT_TRUE(texture_array->Set(1, packed_pixels[1]));
  EXPECT_THAT(test_texture_array->GetPixels(0), ElementsAreArray(pixels[0]));
  EXPECT_THAT(test_texture_array->GetPackedPixels(1),
              ElementsAreArray(packed_pixels[1]));

  EXPECT_TRUE(texture_array->Set(0, pixels[0].data(),
                                 pixels[0].size() * sizeof(Pixel)));
  EXPECT_THAT(test_texture_array->GetPixels(0), ElementsAreArray(pixels[0]));

  auto oversize_pixels = MakePixels(kTextureWidth * kTextureHeight + 1,
                                    {0, 0, 0, 0}, {1, 2, 3, 4});
  auto oversize_packed_pixels =
      MakePackedPixels(kTextureWidth * kTextureHeight + 1, 0, 1);
  ASSERT_TRUE(texture_array->Clear(0));

  EXPECT_TRUE(texture_array->Set(0, oversize_pixels));
  EXPECT_THAT(test_texture_array->GetPixels(0), ElementsAreArray(pixels[0]));
  EXPECT_THAT(test_texture_array->GetPackedPixels(1),
              ElementsAreArray(packed_pixels[1]));

  EXPECT_TRUE(texture_array->Set(0, oversize_packed_pixels));
  EXPECT_THAT(test_texture_array->GetPackedPixels(0),
              ElementsAreArray(packed_pixels[0]));
  EXPECT_THAT(test_texture_array->GetPackedPixels(1),
              ElementsAreArray(packed_pixels[1]));

  EXPECT_TRUE(texture_array->Set(0, oversize_pixels.data(),
                                 oversize_pixels.size() * sizeof(Pixel)));
  EXPECT_THAT(test_texture_array->GetPixels(0), ElementsAreArray(pixels[0]));
  EXPECT_THAT(test_texture_array->GetPackedPixels(1),
              ElementsAreArray(packed_pixels[1]));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture_array->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_array->GetModifyCount(), 7);
}

TEST_F(TextureArrayTest, FailSet) {
  CreateSystem();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticWrite, 1, kTextureWidth, kTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());

  EXPECT_FALSE(texture_array->Set(
      0, MakePixels(kTextureWidth * kTextureHeight - 1, {})));
  EXPECT_FALSE(texture_array->Set(
      0, MakePackedPixels(kTextureWidth * kTextureHeight - 1, 0)));
  auto pixels = MakePixels(kTextureWidth * kTextureHeight, {});
  EXPECT_FALSE(
      texture_array->Set(0, pixels.data(), pixels.size() * sizeof(Pixel) - 1));
  EXPECT_FALSE(
      texture_array->Set(-1, MakePixels(kTextureWidth * kTextureHeight, {})));
  EXPECT_FALSE(
      texture_array->Set(1, MakePixels(kTextureWidth * kTextureHeight, {})));

  state_.texture_array_config.fail_set = true;
  EXPECT_FALSE(
      texture_array->Set(0, MakePixels(kTextureWidth * kTextureHeight, {})));
  EXPECT_FALSE(texture_array->Set(
      0, MakePackedPixels(kTextureWidth * kTextureHeight, 0)));
  EXPECT_FALSE(
      texture_array->Set(0, pixels.data(), pixels.size() * sizeof(Pixel)));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture_array->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_array->GetModifyCount(), 0);
}

TEST_F(TextureArrayTest, Get) {
  CreateSystem();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, kTextureCount, kTextureWidth,
      kTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());

  EXPECT_TRUE(texture_array->Set(0, MakePixels(kTextureWidth * kTextureHeight,
                                               {0, 0, 0, 0}, {1, 2, 3, 4})));
  EXPECT_TRUE(texture_array->Set(1, MakePixels(kTextureWidth * kTextureHeight,
                                               {0, 0, 0, 0}, {4, 3, 2, 1})));

  std::vector<Pixel> pixels;
  ASSERT_TRUE(texture_array->Get(0, pixels));
  EXPECT_THAT(test_texture_array->GetPixels(0), ElementsAreArray(pixels));
  ASSERT_TRUE(texture_array->Get(1, pixels));
  EXPECT_THAT(test_texture_array->GetPixels(1), ElementsAreArray(pixels));

  std::vector<uint32_t> packed_pixels;
  ASSERT_TRUE(texture_array->Get(0, packed_pixels));
  EXPECT_THAT(test_texture_array->GetPackedPixels(0),
              ElementsAreArray(packed_pixels));
  ASSERT_TRUE(texture_array->Get(1, packed_pixels));
  EXPECT_THAT(test_texture_array->GetPackedPixels(1),
              ElementsAreArray(packed_pixels));

  packed_pixels = std::vector<uint32_t>(kTextureWidth * kTextureHeight, 0);
  ASSERT_TRUE(texture_array->Get(0, packed_pixels.data(),
                                 packed_pixels.size() * sizeof(uint32_t)));
  EXPECT_THAT(test_texture_array->GetPackedPixels(0),
              ElementsAreArray(packed_pixels));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture_array->GetInvalidCallCount(), 0);
}

TEST_F(TextureArrayTest, FailGet) {
  CreateSystem();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, 1, kTextureWidth, kTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());

  std::vector<Pixel> pixels(kTextureWidth * kTextureHeight, Pixel(0, 0, 0));
  std::vector<uint32_t> packed_pixels(kTextureWidth * kTextureHeight, 0);
  EXPECT_FALSE(texture_array->Get(0, packed_pixels.data(),
                                  packed_pixels.size() * sizeof(uint32_t) - 1));

  state_.texture_array_config.fail_get = true;
  EXPECT_FALSE(texture_array->Get(0, pixels));
  EXPECT_FALSE(texture_array->Get(0, packed_pixels));
  EXPECT_FALSE(texture_array->Get(0, packed_pixels.data(),
                                  packed_pixels.size() * sizeof(uint32_t)));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture_array->GetInvalidCallCount(), 0);
}

TEST_F(TextureArrayTest, CannotGetStaticWrite) {
  CreateSystem();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticWrite, 1, kTextureWidth, kTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());

  std::vector<Pixel> pixels;
  EXPECT_FALSE(texture_array->Get(0, pixels));

  std::vector<uint32_t> packed_pixels;
  EXPECT_FALSE(texture_array->Get(0, packed_pixels));

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_texture_array->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_texture_array->GetModifyCount(), 0);
}

}  // namespace
}  // namespace gb
