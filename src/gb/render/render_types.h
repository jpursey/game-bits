// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_TYPES_H_
#define GB_RENDER_RENDER_TYPES_H_

#include <vector>

#include "absl/types/span.h"
#include "gb/base/access_token.h"
#include "gb/base/flags.h"
#include "gb/base/type_info.h"

namespace gb {

class BindingData;
class LocalBindingData;
class Material;
class MaterialType;
class Mesh;
class MeshView;
class RenderBackend;
class RenderBuffer;
class RenderBufferView;
class RenderSystem;
class RenderPipeline;
class RenderScene;
class RenderSceneType;
class RenderTest;
class Shader;
class ShaderCode;
class Texture;
class TextureView;
struct Binding;

// Internal access token for functions callable by render classes.
GB_DEFINE_ACCESS_TOKEN(RenderInternal, class RenderSystem,
                       class LocalBindingData, class Material, class Mesh,
                       class RenderFrame, class RenderBuffer,
                       class RenderSceneType, class Texture, class RenderTest);

//==============================================================================
// FrameDimensions
//==============================================================================

// Represents the dimensions for a frame.
struct FrameDimensions {
  int width;
  int height;
};

//==============================================================================
// RenderDataType
//==============================================================================

// Describes a game defined structured data type used for shader constants,
// vertex definitions, etc.
class RenderDataType final {
 public:
  RenderDataType(RenderInternal, TypeKey* type, size_t size)
      : type_(type), size_(static_cast<int>(size)) {}

  TypeKey* GetType() const { return type_; }
  int GetSize() const { return size_; }

 private:
  TypeKey* type_ = nullptr;
  int size_ = 0;
};

//==============================================================================
// ShaderType
//==============================================================================

// A shader type specifies where in a in the render pipeline a shader is
// executed.
enum class ShaderType : int {
  kVertex,
  kFragment,
};

using ShaderTypes = Flags<ShaderType>;
inline constexpr ShaderTypes kAllShaderTypes = {
    ShaderType::kVertex,
    ShaderType::kFragment,
};

//==============================================================================
// ShaderValue / ShaderParam
//==============================================================================

// A shader value specifies in/out types expected/provided by a shader.
enum class ShaderValue : int {
  kFloat,
  kVec2,
  kVec3,
  kVec4,
};

// A shader parameter binds a ShaderValue to a shader pipeline input or output
// location.
struct ShaderParam {
  constexpr ShaderParam() = default;
  constexpr ShaderParam(ShaderValue value, int location)
      : value(value), location(location) {}

  ShaderValue value = ShaderValue::kFloat;
  int location = 0;
};

inline constexpr bool operator==(const ShaderParam& a, const ShaderParam& b) {
  return a.value == b.value && a.location == b.location;
}
inline constexpr bool operator!=(const ShaderParam& a, const ShaderParam& b) {
  return !(a == b);
}
inline constexpr bool operator<(const ShaderParam& a, const ShaderParam& b) {
  return a.location < b.location ||
         (a.location == b.location && a.value < b.value);
}
inline constexpr bool operator>(const ShaderParam& a, const ShaderParam& b) {
  return b < a;
}
inline constexpr bool operator<=(const ShaderParam& a, const ShaderParam& b) {
  return !(b < a);
}
inline constexpr bool operator>=(const ShaderParam& a, const ShaderParam& b) {
  return !(a < b);
}

//==============================================================================
// VertexType
//==============================================================================

// A vertex type describes the attributes on a vertex.
//
// Vertex types are expected to be packed.
class VertexType final {
 public:
  VertexType(RenderInternal, TypeKey* type, size_t size,
             absl::Span<const ShaderValue> attributes)
      : type_(type),
        size_(static_cast<int>(size)),
        attributes_(attributes.begin(), attributes.end()) {}

  TypeKey* GetType() const { return type_; }
  int GetSize() const { return size_; }
  absl::Span<const ShaderValue> GetAttributes() const { return attributes_; }

 private:
  TypeKey* type_ = nullptr;
  int size_ = 0;
  std::vector<ShaderValue> attributes_;
};

//==============================================================================
// DataVolatility
//==============================================================================

// Data volatility specifies how often data may be updated when rendering.
enum class DataVolatility : int {
  kStaticWrite,      // Data is rarely changed, and can only be written to.
  kStaticReadWrite,  // Data is rarely changed, but is editable. May require up
                     // to 2x memory over kStaticWrite.
  kPerFrame,         // Data is often changed per-frame. May require up to
                     // 3x to 5x memory over kStaticWrite.
};

//==============================================================================
// BindingType
//==============================================================================

// A binding type specifies what kind of resource is or may be bound within a
// binding set.
enum class BindingType : int {
  kNone,       // No binding.
  kConstants,  // Binds structured constant data.
  kTexture,    // Binds a texture.
};

//==============================================================================
// BindingSet
//==============================================================================

// A binding set describes a game-defined set of resource bindings which are
// passed to shaders in a material type.
enum class BindingSet : int {
  // The scene binding set is global data shared across the entire scene.
  kScene = 0,

  // The material binding set is defined at the material type level, and can be
  // overridden per material.
  kMaterial = 1,

  // The instance binding set is for potentially volatile material-specific data
  // that may be different per mesh instance.
  kInstance = 2,
};

//==============================================================================
// Triangle
//==============================================================================

// Represents a triangle as three 16-bit indices into an index buffer.
struct alignas(uint16_t) Triangle {
  constexpr Triangle() = default;
  constexpr Triangle(uint16_t a, uint16_t b, uint16_t c) : a(a), b(b), c(c) {}

  uint16_t a = 0;
  uint16_t b = 0;
  uint16_t c = 0;
};
static_assert(sizeof(Triangle) == sizeof(uint16_t) * 3,
              "Triangle must be equivalent to three uint16_t values");

inline constexpr bool operator==(const Triangle& i, const Triangle& j) {
  return i.a == j.a && i.b == j.b && i.c == j.c;
}
inline constexpr bool operator!=(const Triangle& i, const Triangle& j) {
  return !(i == j);
}

//==============================================================================
// Pixel
//==============================================================================

// Represents an RGBA pixel.
struct alignas(uint32_t) Pixel {
  // Transparent black pixel.
  constexpr Pixel() = default;

  // Default opaque colored pixel.
  constexpr Pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
      : r(r), g(g), b(b), a(a) {}

  // Direct from a packed pixel value.
  explicit constexpr Pixel(uint32_t packed)
      : r(reinterpret_cast<const uint8_t*>(&packed)[0]),
        g(reinterpret_cast<const uint8_t*>(&packed)[1]),
        b(reinterpret_cast<const uint8_t*>(&packed)[2]),
        a(reinterpret_cast<const uint8_t*>(&packed)[3]) {}

  // Returns the pixel in packed form.
  constexpr uint32_t Packed() const {
    return *reinterpret_cast<const uint32_t*>(this);
  }

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
};
static_assert(sizeof(Pixel) == sizeof(uint32_t), "Pixel must be 4 bytes");

inline constexpr bool operator==(const Pixel& a, const Pixel& b) {
  return a.Packed() == b.Packed();
}
inline constexpr bool operator!=(const Pixel& a, const Pixel& b) {
  return !(a == b);
}

//==============================================================================
// Constants
//==============================================================================

// This defines the upper limit for a binding index for a Binding.
inline constexpr int kMaxBindingIndex = 1023;

}  // namespace gb

#endif  // GB_RENDER_RENDER_TYPES_H_
