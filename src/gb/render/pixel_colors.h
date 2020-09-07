// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_PIXEL_COLORS_H_
#define GB_RENDER_PIXEL_COLORS_H_

#include "gb/render/render_types.h"

namespace gb {

namespace Colors {

inline constexpr Pixel kBlack(0, 0, 0);
inline constexpr Pixel kBlue(0, 0, 255);
inline constexpr Pixel kCyan(0, 255, 255);
inline constexpr Pixel kGreen(0, 255, 0);
inline constexpr Pixel kGrey(128, 128, 128);
inline constexpr Pixel kMagenta(255, 0, 255);
inline constexpr Pixel kRed(255, 0, 0);
inline constexpr Pixel kWhite(255, 255, 255);
inline constexpr Pixel kYellow(255, 255, 0);

}  // namespace Colors

}  // namespace gb

#endif  // GB_RENDER_PIXEL_COLORS_H_
