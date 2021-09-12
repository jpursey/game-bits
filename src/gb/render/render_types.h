// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_TYPES_H_
#define GB_RENDER_RENDER_TYPES_H_

#include <algorithm>
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
class TextureArray;
struct Binding;
struct DrawCommand;
struct MaterialConfig;
struct SamplerOptions;

// Resource chunks
namespace fbs {
struct MeshChunk;
struct MaterialChunk;
struct MaterialTypeChunk;
struct ShaderChunk;
struct TextureChunk;
struct TextureArrayChunk;
}  // namespace fbs

// Internal access token for functions callable by render classes.
GB_BEGIN_ACCESS_TOKEN(RenderInternal)
friend class RenderSystem;
friend class LocalBindingData;
friend class Material;
friend class Mesh;
friend class RenderFrame;
friend class RenderBuffer;
friend class RenderSceneType;
friend class Texture;
friend class DrawList;
friend class RenderTest;
GB_END_ACCESS_TOKEN()

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
  RenderDataType(RenderInternal, std::string_view name, TypeKey* type,
                 size_t size)
      : name_(name.data(), name.size()),
        type_(type),
        size_(static_cast<int>(size)) {}

  const std::string& GetName() const { return name_; }
  TypeKey* GetType() const { return type_; }
  int GetSize() const { return size_; }

 private:
  std::string name_;
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
enum class ShaderValue : int32_t {
  kFloat,
  kVec2,
  kVec3,
  kVec4,
  kColor,
};

// A shader parameter binds a ShaderValue to a shader pipeline input or output
// location.
struct ShaderParam {
  constexpr ShaderParam() = default;
  constexpr ShaderParam(ShaderValue value, int location)
      : value(value), location(location) {}

  ShaderValue value = ShaderValue::kFloat;
  int32_t location = 0;
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
  VertexType(RenderInternal, std::string_view name, TypeKey* type, size_t size,
             absl::Span<const ShaderValue> attributes)
      : name_(name.data(), name.size()),
        type_(type),
        size_(static_cast<int>(size)),
        attributes_(attributes.begin(), attributes.end()) {}

  const std::string& GetName() const { return name_; }
  TypeKey* GetType() const { return type_; }
  int GetSize() const { return size_; }
  absl::Span<const ShaderValue> GetAttributes() const { return attributes_; }

 private:
  std::string name_;
  TypeKey* type_ = nullptr;
  int size_ = 0;
  std::vector<ShaderValue> attributes_;
};

//==============================================================================
// DataVolatility
//==============================================================================

// Data volatility specifies how often data may be updated when rendering.
enum class DataVolatility : int {
  kInvalid,          // Invalid value for DataVolatility
  kStaticWrite,      // Data is rarely changed, and can only be written to.
  kStaticReadWrite,  // Data is rarely changed, but is editable. May require up
                     // to 2x memory over kStaticWrite.
  kPerFrame,         // Data is often changed per-frame. May require up to
                     // 3x or more memory over kStaticWrite.
};

//==============================================================================
// BindingType
//==============================================================================

// A binding type specifies what kind of resource is or may be bound within a
// binding set.
enum class BindingType : int {
  kNone,          // No binding.
  kConstants,     // Binds structured constant data.
  kTexture,       // Binds a texture.
  kTextureArray,  // Binds a texture array.
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

inline constexpr Triangle operator+(const Triangle& triangle, int i) {
  return Triangle(triangle.a + i, triangle.b + i, triangle.c + i);
}
inline constexpr bool operator==(const Triangle& i, const Triangle& j) {
  return i.a == j.a && i.b == j.b && i.c == j.c;
}
inline constexpr bool operator!=(const Triangle& i, const Triangle& j) {
  return !(i == j);
}

//==============================================================================
// Constants
//==============================================================================

// This defines the upper limit for a binding index for a Binding.
inline constexpr int kMaxBindingIndex = 1023;

// Maximum dimensions for a texture or texture array.
inline constexpr int kMaxTextureWidth = 8096;
inline constexpr int kMaxTextureHeight = 8096;

// Maximum count and total pixel count for a texture array.
inline constexpr int kMaxTextureArrayCount = 2048;
inline constexpr int kMaxTextureArrayPixels = 512 * 1024 * 1024;

}  // namespace gb

#endif  // GB_RENDER_RENDER_TYPES_H_
