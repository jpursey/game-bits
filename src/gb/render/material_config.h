// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_MATERIAL_CONFIG_H_
#define GB_RENDER_MATERIAL_CONFIG_H_

#include "gb/base/flags.h"
#include "gb/render/render_types.h"

namespace gb {

// Determines how a material interacts with the depth buffer.
enum class DepthMode {
  kNone,          // No depth test or write
  kTest,          // Depth test is done, but the depth buffer is not modified.
  kWrite,         // Depth write is done always, but without a depth test.
  kTestAndWrite,  // Depth test is done and depth is updated.
};

// If DepthMode is kTest or kTestAndWrite, the DepthTest value specifies how the
// depth value of a new fragment will compare to what is in the depth buffer.
enum class DepthTest {
  kLess,
  kLessOrEqual,
  kEqual,
  kGreaterOrEqual,
  kGreater,
};

// Determines how polygon primitives are rasterized.
enum class RasterMode {
  kFill,  // Polygons are filled in.
  kLine,  // Polygons render only line (aka wireframe).
};

// Determines how faces are culled, given CCW winding
enum class CullMode {
  kNone,   // No culling is done, both the front and back face are drawn.
  kFront,  // Culls front face.
  kBack,   // Culls back face.
};

// Determines how a fragment's color is combined with the color already drawn.
enum class BlendMode {
  // The fragment's color and alpha replace what is there.
  kNone,

  // The fragment's color is blended over what is there by its alpha, and its
  // alpha replaces what is there.
  kAlpha,

  // The fragment's color is already multiplied by its alpha, and is added to
  // what is there scaled by one minus its alpha. Alpha is combined the same
  // way.
  kPremultiplied,

  // The fragment's color, scaled by its alpha, is added to what is there. The
  // alpha that is there is kept.
  kAdditive,
};

// A color channel that a material writes.
enum class ColorChannel {
  kRed,
  kGreen,
  kBlue,
  kAlpha,
};

using ColorChannels = Flags<ColorChannel>;
inline constexpr ColorChannels kAllColorChannels = {
    ColorChannel::kRed,
    ColorChannel::kGreen,
    ColorChannel::kBlue,
    ColorChannel::kAlpha,
};

// MaterialConfig defines behavior of how mesh using a material is drawn by the
// renderer.
//
// This is set as part of a material type.
struct MaterialConfig {
  // Initialize with the standard material parameters (see member defaults).
  MaterialConfig() = default;

  // Helper functions for constructing MaterialConfig as a temporary.
  MaterialConfig& SetDepthMode(DepthMode mode,
                               DepthTest test = DepthTest::kLess) {
    depth_mode = mode;
    depth_test = test;
    return *this;
  }
  MaterialConfig& SetRasterMode(RasterMode mode) {
    raster_mode = mode;
    return *this;
  }
  MaterialConfig& SetCullMode(CullMode mode) {
    cull_mode = mode;
    return *this;
  }
  MaterialConfig& SetBlendMode(BlendMode mode) {
    blend_mode = mode;
    return *this;
  }

  // With no channels, a material writes only depth, as in a depth pre-pass.
  MaterialConfig& SetColorChannels(ColorChannels channels) {
    color_channels = channels;
    return *this;
  }

  // Adds to the depth of each fragment, as shadow maps and decals need to
  // avoid fighting with the surface under them. `constant` is scaled by the
  // smallest depth difference the depth buffer can resolve at the polygon's
  // depth, and `slope` by how steeply the polygon's depth changes across a
  // pixel.
  MaterialConfig& SetDepthBias(float constant, float slope) {
    depth_bias_constant = constant;
    depth_bias_slope = slope;
    return *this;
  }

  bool operator==(const MaterialConfig&) const = default;

  DepthMode depth_mode = DepthMode::kTestAndWrite;
  DepthTest depth_test = DepthTest::kLess;
  RasterMode raster_mode = RasterMode::kFill;
  CullMode cull_mode = CullMode::kBack;
  BlendMode blend_mode = BlendMode::kAlpha;
  ColorChannels color_channels = kAllColorChannels;
  float depth_bias_constant = 0.0f;
  float depth_bias_slope = 0.0f;
};

}  // namespace gb

#endif  // GB_RENDER_MATERIAL_CONFIG_H_
