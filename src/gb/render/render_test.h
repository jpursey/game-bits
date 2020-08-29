// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_TEST_H_
#define GB_RENDER_RENDER_TEST_H_

#include <ostream>
#include <string_view>

#include "absl/types/span.h"
#include "gb/file/file_system.h"
#include "gb/render/material_config.h"
#include "gb/render/render_system.h"
#include "gb/render/test_render_backend.h"
#include "gb/resource/resource_system.h"
#include "gtest/gtest.h"

namespace gb {

// This class is here to provide common functionality across all render tests.
class RenderTest : public ::testing::Test {
 protected:
  // Test types.
  struct Vector2 {
    float x;
    float y;

    bool operator==(const Vector2& other) const {
      return x == other.x && y == other.y;
    }
    friend inline void PrintTo(const Vector2& value, std::ostream* out) {
      *out << "{" << value.x << ", " << value.y << "}";
    }
  };
  struct Vector3 {
    float x;
    float y;
    float z;

    bool operator==(const Vector3& other) const {
      return x == other.x && y == other.y && z == other.z;
    }
    friend inline void PrintTo(const Vector3& value, std::ostream* out) {
      *out << "{" << value.x << ", " << value.y << ", " << value.z << "}";
    }
  };
  struct Vector4 {
    float x;
    float y;
    float z;
    float w;

    bool operator==(const Vector4& other) const {
      return x == other.x && y == other.y && z == other.z && w == other.w;
    }
    friend inline void PrintTo(const Vector4& value, std::ostream* out) {
      *out << "{" << value.x << ", " << value.y << ", " << value.z << ", "
           << value.w << "}";
    }
  };

  // Test constants.
  static inline constexpr std::string_view kVertexShaderCode = "vertex";
  static inline constexpr std::string_view kFragmentShaderCode = "fragment";

  // Mints a render module internal access token, which tests can use to ensure
  // internal functions are working as designed.
  RenderInternal GetAccessToken() { return {}; }

  // Creates a RenderSystem with a resource system, memory-backed file system
  // and test backend, storing in render_system_.
  void CreateSystem(bool edit_mode = false);

  // Creates a test pipeline with the Vector3 vertex data and the requested
  // bindings.
  std::unique_ptr<RenderPipeline> CreatePipeline(
      absl::Span<const Binding> bindings, const MaterialConfig& config);

  // Creates a test material type with the Vector3 vertex data and the requested
  // bindings.
  MaterialType* CreateMaterialType(absl::Span<const Binding> bindings,
                                   const MaterialConfig& config = {});

  // Creates a test material with the Vector3 vertex data and the requested
  // bindings.
  Material* CreateMaterial(absl::Span<const Binding> bindings);

  // Data used in conjunction with CreateSystem();
  TestRenderBackend::State state_;
  std::unique_ptr<gb::ResourceSystem> resource_system_;
  std::unique_ptr<gb::FileSystem> file_system_;
  std::unique_ptr<gb::RenderSystem> render_system_;

  // Holds temporary resources from Create* methods.
  ResourceSet temp_resource_set_;
};

// Helpers for test output of basic values
inline void PrintTo(const Triangle& value, std::ostream* out) {
  *out << "{" << value.a << ", " << value.b << ", " << value.c << "}";
}
inline void PrintTo(const Pixel& value, std::ostream* out) {
  *out << "{" << static_cast<int>(value.r) << ", " << static_cast<int>(value.g)
       << ", " << static_cast<int>(value.b) << ", " << static_cast<int>(value.a)
       << "}";
}

}  // namespace gb

#endif  // GB_RENDER_RENDER_TEST_H_
