// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_MATERIAL_CONFIG_H_
#define GB_RENDER_MATERIAL_CONFIG_H_

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

  DepthMode depth_mode = DepthMode::kTestAndWrite;
  DepthTest depth_test = DepthTest::kLess;
  RasterMode raster_mode = RasterMode::kFill;
  CullMode cull_mode = CullMode::kBack;
};

}  // namespace gb

#endif  // GB_RENDER_MATERIAL_CONFIG_H_