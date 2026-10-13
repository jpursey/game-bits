// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMAGE_PIXEL_H_
#define GB_IMAGE_PIXEL_H_

#include <stdint.h>

#include <bit>

#include "gb/image/image_types.h"
#include "glm/glm.hpp"

namespace gb {

namespace internal {

// Converts a value to a pixel channel, clamping it to [0, 255]. NaN becomes 0.
constexpr uint8_t ToPixelChannel(double value) {
  if (!(value > 0.0)) {
    return 0;
  }
  return static_cast<uint8_t>(value < 255.0 ? value : 255.0);
}

}  // namespace internal

// Represents an RGBA pixel.
struct alignas(uint32_t) Pixel {
  // Transparent black pixel.
  constexpr Pixel() = default;

  // Default opaque colored pixel.
  constexpr Pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
      : r(r), g(g), b(b), a(a) {}

  // From a color, with each channel clamped to [0, 1] (NaN becomes 0).
  constexpr explicit Pixel(const glm::vec3& color)
      : r(internal::ToPixelChannel(color.r * 255.0f)),
        g(internal::ToPixelChannel(color.g * 255.0f)),
        b(internal::ToPixelChannel(color.b * 255.0f)),
        a(255) {}
  constexpr explicit Pixel(const glm::vec4& color)
      : r(internal::ToPixelChannel(color.r * 255.0f)),
        g(internal::ToPixelChannel(color.g * 255.0f)),
        b(internal::ToPixelChannel(color.b * 255.0f)),
        a(internal::ToPixelChannel(color.a * 255.0f)) {}

  // Direct from a packed pixel value.
  constexpr explicit Pixel(uint32_t packed)
      : Pixel(std::bit_cast<Pixel>(packed)) {}

  // Returns the pixel in packed form: its bytes in memory as a uint32_t (red is
  // the lowest byte on little-endian machines).
  constexpr uint32_t Packed() const { return std::bit_cast<uint32_t>(*this); }

  // Create a new Pixel based on this color but with a new alpha.
  constexpr Pixel WithAlpha(uint8_t new_a) const {
    return Pixel(r, g, b, new_a);
  }

  // Create a new Pixel based on this color with its alpha scaled by `mod`,
  // clamped to [0, 255] (NaN becomes 0).
  constexpr Pixel ModAlpha(float mod) const {
    return Pixel(r, g, b, internal::ToPixelChannel(a * mod));
  }
  constexpr Pixel ModAlpha(double mod) const {
    return Pixel(r, g, b, internal::ToPixelChannel(a * mod));
  }

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
};
static_assert(sizeof(Pixel) == sizeof(uint32_t), "Pixel must be 4 bytes");

inline constexpr bool operator==(Pixel a, Pixel b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
inline constexpr bool operator!=(Pixel a, Pixel b) { return !(a == b); }

// Scales the color of a pixel, leaving its alpha unchanged. Each channel is
// clamped to [0, 255] (NaN becomes 0).
inline constexpr Pixel operator*(Pixel pixel, float mod) {
  return Pixel(internal::ToPixelChannel(pixel.r * mod),
               internal::ToPixelChannel(pixel.g * mod),
               internal::ToPixelChannel(pixel.b * mod), pixel.a);
}
inline constexpr Pixel operator*(float mod, Pixel pixel) { return pixel * mod; }
inline constexpr Pixel operator*(Pixel pixel, double mod) {
  return Pixel(internal::ToPixelChannel(pixel.r * mod),
               internal::ToPixelChannel(pixel.g * mod),
               internal::ToPixelChannel(pixel.b * mod), pixel.a);
}
inline constexpr Pixel operator*(double mod, Pixel pixel) {
  return pixel * mod;
}

namespace PixelColor {

inline constexpr Pixel kTransparent(0, 0, 0, 0);
inline constexpr Pixel kBlack(0, 0, 0);
inline constexpr Pixel kBlue(0, 0, 255);
inline constexpr Pixel kCyan(0, 255, 255);
inline constexpr Pixel kGreen(0, 255, 0);
inline constexpr Pixel kGrey(128, 128, 128);
inline constexpr Pixel kMagenta(255, 0, 255);
inline constexpr Pixel kRed(255, 0, 0);
inline constexpr Pixel kWhite(255, 255, 255);
inline constexpr Pixel kYellow(255, 255, 0);

}  // namespace PixelColor

}  // namespace gb

#endif  // GB_IMAGE_PIXEL_H_
