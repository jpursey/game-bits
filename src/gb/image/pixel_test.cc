// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/image/pixel.h"

#include <limits>

#include "gtest/gtest.h"

namespace gb {
namespace {

constexpr float kFloatNaN = std::numeric_limits<float>::quiet_NaN();
constexpr double kDoubleNaN = std::numeric_limits<double>::quiet_NaN();

TEST(PixelTest, Construct) {
  EXPECT_EQ(Pixel(), Pixel(0, 0, 0, 0));
  EXPECT_EQ(Pixel(1, 2, 3), Pixel(1, 2, 3, 255));

  const Pixel pixel(1, 2, 3, 4);
  EXPECT_EQ(pixel.r, 1);
  EXPECT_EQ(pixel.g, 2);
  EXPECT_EQ(pixel.b, 3);
  EXPECT_EQ(pixel.a, 4);
}

TEST(PixelTest, Packed) {
  const Pixel pixel(0x04030201u);
  EXPECT_EQ(pixel, Pixel(1, 2, 3, 4));
  EXPECT_EQ(pixel.Packed(), 0x04030201u);
  static_assert(Pixel(0x04030201u).Packed() == 0x04030201u);
}

TEST(PixelTest, ConstructFromColor) {
  EXPECT_EQ(Pixel(glm::vec3(1.0f, 0.0f, 1.0f)), Pixel(255, 0, 255, 255));
  EXPECT_EQ(Pixel(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)), Pixel(0, 255, 0, 255));
  EXPECT_EQ(Pixel(glm::vec4(0.0f)), Pixel(0, 0, 0, 0));
}

TEST(PixelTest, ConstructFromColorRoundTrips) {
  for (int i = 0; i < 256; ++i) {
    const float value = static_cast<float>(i) / 255.0f;
    const uint8_t expected = static_cast<uint8_t>(i);
    EXPECT_EQ(Pixel(glm::vec4(value)),
              Pixel(expected, expected, expected, expected))
        << "Value: " << i;
  }
}

TEST(PixelTest, ConstructFromColorOutOfRange) {
  EXPECT_EQ(Pixel(glm::vec3(2.0f, -1.0f, 1.5f)), Pixel(255, 0, 255, 255));
  EXPECT_EQ(Pixel(glm::vec4(-0.5f, 3.0f, -2.0f, 1.01f)), Pixel(0, 255, 0, 255));
}

TEST(PixelTest, NaNBecomesZero) {
  EXPECT_EQ(Pixel(glm::vec3(kFloatNaN)), Pixel(0, 0, 0, 255));
  EXPECT_EQ(Pixel(glm::vec4(kFloatNaN)), Pixel(0, 0, 0, 0));
  EXPECT_EQ(Pixel(1, 2, 3, 4).ModAlpha(kFloatNaN), Pixel(1, 2, 3, 0));
  EXPECT_EQ(Pixel(1, 2, 3, 4).ModAlpha(kDoubleNaN), Pixel(1, 2, 3, 0));
  EXPECT_EQ(Pixel(1, 2, 3, 4) * kFloatNaN, Pixel(0, 0, 0, 4));
  EXPECT_EQ(Pixel(1, 2, 3, 4) * kDoubleNaN, Pixel(0, 0, 0, 4));
}

TEST(PixelTest, WithAlpha) {
  EXPECT_EQ(Pixel(1, 2, 3, 4).WithAlpha(5), Pixel(1, 2, 3, 5));
}

TEST(PixelTest, ModAlpha) {
  EXPECT_EQ(Pixel(1, 2, 3, 100).ModAlpha(0.5f), Pixel(1, 2, 3, 50));
  EXPECT_EQ(Pixel(1, 2, 3, 100).ModAlpha(0.5), Pixel(1, 2, 3, 50));
  EXPECT_EQ(Pixel(1, 2, 3, 100).ModAlpha(3.0f), Pixel(1, 2, 3, 255));
  EXPECT_EQ(Pixel(1, 2, 3, 100).ModAlpha(3.0), Pixel(1, 2, 3, 255));
  EXPECT_EQ(Pixel(1, 2, 3, 100).ModAlpha(-1.0f), Pixel(1, 2, 3, 0));
  EXPECT_EQ(Pixel(1, 2, 3, 100).ModAlpha(-1.0), Pixel(1, 2, 3, 0));
}

TEST(PixelTest, Multiply) {
  const Pixel pixel(100, 50, 200, 4);
  EXPECT_EQ(pixel * 0.5f, Pixel(50, 25, 100, 4));
  EXPECT_EQ(0.5f * pixel, Pixel(50, 25, 100, 4));
  EXPECT_EQ(pixel * 0.5, Pixel(50, 25, 100, 4));
  EXPECT_EQ(0.5 * pixel, Pixel(50, 25, 100, 4));
  EXPECT_EQ(pixel * 2.0f, Pixel(200, 100, 255, 4));
  EXPECT_EQ(pixel * 2.0, Pixel(200, 100, 255, 4));
  EXPECT_EQ(pixel * -1.0f, Pixel(0, 0, 0, 4));
  EXPECT_EQ(pixel * -1.0, Pixel(0, 0, 0, 4));
}

TEST(PixelTest, Compare) {
  EXPECT_TRUE(Pixel(1, 2, 3, 4) == Pixel(1, 2, 3, 4));
  EXPECT_FALSE(Pixel(1, 2, 3, 4) != Pixel(1, 2, 3, 4));
  EXPECT_NE(Pixel(1, 2, 3, 4), Pixel(0, 2, 3, 4));
  EXPECT_NE(Pixel(1, 2, 3, 4), Pixel(1, 0, 3, 4));
  EXPECT_NE(Pixel(1, 2, 3, 4), Pixel(1, 2, 0, 4));
  EXPECT_NE(Pixel(1, 2, 3, 4), Pixel(1, 2, 3, 0));
}

}  // namespace
}  // namespace gb
