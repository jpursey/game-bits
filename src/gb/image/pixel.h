// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMAGE_PIXEL_H_
#define GB_IMAGE_PIXEL_H_

#include <stdint.h>

#include <algorithm>

#include "gb/image/image_types.h"
#include "glm/glm.hpp"

namespace gb {

// Represents an RGBA pixel.
struct alignas(uint32_t) Pixel {
  // Transparent black pixel.
  constexpr Pixel() = default;

  // Default opaque colored pixel.
  constexpr Pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
      : r(r), g(g), b(b), a(a) {}
  constexpr explicit Pixel(const glm::vec3& color)
      : r(static_cast<uint8_t>(color.r * 255.0f)),
        g(static_cast<uint8_t>(color.g * 255.0f)),
        b(static_cast<uint8_t>(color.b * 255.0f)),
        a(255) {}
  constexpr explicit Pixel(const glm::vec4& color)
      : r(static_cast<uint8_t>(color.r * 255.0f)),
        g(static_cast<uint8_t>(color.g * 255.0f)),
        b(static_cast<uint8_t>(color.b * 255.0f)),
        a(static_cast<uint8_t>(color.a * 255.0f)) {}

  // Direct from a packed pixel value.
  explicit Pixel(uint32_t packed)
      : r(reinterpret_cast<const uint8_t*>(&packed)[0]),
        g(reinterpret_cast<const uint8_t*>(&packed)[1]),
        b(reinterpret_cast<const uint8_t*>(&packed)[2]),
        a(reinterpret_cast<const uint8_t*>(&packed)[3]) {}

  // Returns the pixel in packed form.
  uint32_t Packed() const { return *reinterpret_cast<const uint32_t*>(this); }

  // Create a new Pixel based on this color but with a new alpha.
  constexpr Pixel WithAlpha(uint8_t new_a) const {
    return Pixel(r, g, b, new_a);
  }
  constexpr Pixel ModAlpha(float mod) const {
    return Pixel(r, g, b,
                 static_cast<uint8_t>(std::clamp(a * mod, 0.0f, 255.0f)));
  }
  constexpr Pixel ModAlpha(double mod) const {
    return Pixel(r, g, b,
                 static_cast<uint8_t>(std::clamp(a * mod, 0.0, 255.0)));
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
inline constexpr Pixel operator*(Pixel pixel, float mod) {
  return Pixel(static_cast<uint8_t>(std::clamp(pixel.r * mod, 0.0f, 255.0f)),
               static_cast<uint8_t>(std::clamp(pixel.g * mod, 0.0f, 255.0f)),
               static_cast<uint8_t>(std::clamp(pixel.b * mod, 0.0f, 255.0f)),
               pixel.a);
}
inline constexpr Pixel operator*(float mod, Pixel pixel) { return pixel * mod; }
inline constexpr Pixel operator*(Pixel pixel, double mod) {
  return Pixel(static_cast<uint8_t>(std::clamp(pixel.r * mod, 0.0, 255.0)),
               static_cast<uint8_t>(std::clamp(pixel.g * mod, 0.0, 255.0)),
               static_cast<uint8_t>(std::clamp(pixel.b * mod, 0.0, 255.0)),
               pixel.a);
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
