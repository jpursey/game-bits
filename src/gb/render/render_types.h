// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_TYPES_H_
#define GB_RENDER_RENDER_TYPES_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "absl/types/span.h"
#include "gb/base/access_token.h"
#include "gb/base/flags.h"
#include "gb/base/type_info.h"
#include "gb/image/pixel.h"

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
class StorageBuffer;
class Texture;
class TextureArray;
class VolumeTexture;
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
struct StorageBufferChunk;
struct TextureChunk;
struct TextureArrayChunk;
struct VolumeTextureChunk;
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

// A shader type specifies where in the render pipeline a shader is
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
  //             C++           Shader   Conversion
  //             ------------- -------- ---------------------------
  kFloat,     // float         float    direct
  kVec2,      // glm::vec2     vec2     direct
  kVec3,      // glm::vec3     vec3     direct
  kVec4,      // glm::vec4     vec4     direct
  kColor,     // gb::Pixel     vec4     [0, 255] to [0, 1]
  kI8Norm2,   // glm::i8vec2   vec2     [-127, 127] to [-1, 1]
  kI8Norm3,   // glm::i8vec3   vec3     [-127, 127] to [-1, 1]
  kI8Norm4,   // glm::i8vec4   vec4     [-127, 127] to [-1, 1]
  kI16Norm2,  // glm::i16vec2  vec2     [-32767, 32767] to [-1, 1]
  kI16Norm3,  // glm::i16vec3  vec3     [-32767, 32767] to [-1, 1]
  kI16Norm4,  // glm::i16vec4  vec4     [-32767, 32767] to [-1, 1]
  kU8Norm2,   // glm::u8vec2   vec2     [0, 255] to [0, 1]
  kU8Norm3,   // glm::u8vec3   vec3     [0, 255] to [0, 1]
  kU8Norm4,   // glm::u8vec4   vec4     [0, 255] to [0, 1]
  kU16Norm2,  // glm::u16vec2  vec2     [0, 65535] to [0, 1]
  kU16Norm3,  // glm::u16vec3  vec3     [0, 65535] to [0, 1]
  kU16Norm4,  // glm::u16vec4  vec4     [0, 65535] to [0, 1]
  kInt8,      // int8_t        int      direct
  kI8Vec2,    // glm::i8vec2   ivec2    direct
  kI8Vec3,    // glm::i8vec3   ivec3    direct
  kI8Vec4,    // glm::i8vec4   ivec4    direct
  kInt16,     // int16_t       int      direct
  kI16Vec2,   // glm::i16vec2  ivec2    direct
  kI16Vec3,   // glm::i16vec3  ivec3    direct
  kI16Vec4,   // glm::i16vec4  ivec4    direct
  kInt,       // int32_t       int      direct
  kIVec2,     // glm::ivec2    ivec2    direct
  kIVec3,     // glm::ivec3    ivec3    direct
  kIVec4,     // glm::ivec4    ivec4    direct
  kUint8,     // uint8_t       uint     direct
  kU8Vec2,    // glm::u8vec2   uvec2    direct
  kU8Vec3,    // glm::u8vec3   uvec3    direct
  kU8Vec4,    // glm::u8vec4   uvec4    direct
  kUint16,    // uint16_t      uint     direct
  kU16Vec2,   // glm::u16vec2  uvec2    direct
  kU16Vec3,   // glm::u16vec3  uvec3    direct
  kU16Vec4,   // glm::u16vec4  uvec4    direct
  kUint,      // uint32_t      uint     direct
  kUVec2,     // glm::uvec2    uvec2    direct
  kUVec3,     // glm::uvec3    uvec3    direct
  kUVec4,     // glm::uvec4    uvec4    direct
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
// TextureFormat
//==============================================================================

// The format of each texel in a texture.
//
// kRgba8Srgb is color, which shaders read converted from sRGB to linear. The
// rest are read as stored, with each channel scaled to [0, 1].
enum class TextureFormat : uint8_t {
  kRgba8Srgb,  // Four 8-bit channels, sRGB color, as Pixel. 4 bytes.
  kRgba8,      // Four 8-bit channels, as Pixel. 4 bytes.
  kRg8,        // Two 8-bit channels, red first. 2 bytes.
  kR8,         // One 8-bit channel. 1 byte.
  kRgba4,      // Four 4-bit channels in a uint16_t: red in the top 4 bits,
               // then green, blue, and alpha. 2 bytes.
};

// Returns the size of one texel in bytes, or zero for an invalid format.
inline constexpr int GetTexelSize(TextureFormat format) {
  switch (format) {
    case TextureFormat::kRgba8Srgb:
    case TextureFormat::kRgba8:
      return 4;
    case TextureFormat::kRg8:
    case TextureFormat::kRgba4:
      return 2;
    case TextureFormat::kR8:
      return 1;
  }
  return 0;
}

// Returns the size in bytes of a texture's texels.
inline constexpr int64_t GetTextureSizeInBytes(TextureFormat format, int width,
                                               int height, int depth = 1) {
  return int64_t{width} * height * depth * GetTexelSize(format);
}

// Returns true if texels in the format are Pixels.
inline constexpr bool IsPixelFormat(TextureFormat format) {
  return format == TextureFormat::kRgba8Srgb || format == TextureFormat::kRgba8;
}

// Returns the texel for a pixel in the format, in the first
// GetTexelSize(format) bytes of the result. kR8 and kRg8 take the pixel's
// first channels, and kRgba4 the top 4 bits of each.
inline uint32_t PixelToTexel(TextureFormat format, Pixel pixel) {
  if (format == TextureFormat::kRgba4) {
    const uint16_t value =
        static_cast<uint16_t>(((pixel.r >> 4) << 12) | ((pixel.g >> 4) << 8) |
                              ((pixel.b >> 4) << 4) | (pixel.a >> 4));
    uint32_t texel = 0;
    std::memcpy(&texel, &value, sizeof(value));
    return texel;
  }

  // The 8-bit formats are a Pixel's first channels, in order.
  return pixel.Packed();
}

// Writes "count" copies of "texel", which is GetTexelSize(format) bytes, to
// "out_texels".
inline void FillTexels(TextureFormat format, const void* texel, int count,
                       void* out_texels) {
  if (count <= 0) {
    return;
  }
  uint8_t* const out = static_cast<uint8_t*>(out_texels);
  const int64_t size = int64_t{count} * GetTexelSize(format);
  int64_t filled = GetTexelSize(format);
  std::memcpy(out, texel, filled);

  // Each copy doubles what is filled, so there are few calls of any size.
  while (filled < size) {
    const int64_t copy_size = std::min(filled, size - filled);
    std::memcpy(out + filled, out, copy_size);
    filled += copy_size;
  }
}

//==============================================================================
// BindingType
//==============================================================================

// A binding type specifies what kind of resource is or may be bound within a
// binding set.
enum class BindingType : int {
  kNone,           // No binding.
  kConstants,      // Binds structured constant data.
  kTexture,        // Binds a texture.
  kTextureArray,   // Binds a texture array.
  kVolumeTexture,  // Binds a volume texture.
  kStorageBuffer,  // Binds a storage buffer.
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

  uint16_t& operator[](int i) { return i == 0 ? a : (i == 1 ? b : c); }
  const uint16_t& operator[](int i) const {
    return i == 0 ? a : (i == 1 ? b : c);
  }

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

// Maximum number of vertices in a mesh, as indices are 16 bits.
inline constexpr int kMaxMeshVertices = 65535;

// Maximum dimensions for a texture or texture array.
inline constexpr int kMaxTextureWidth = 8192;
inline constexpr int kMaxTextureHeight = 8192;

// Maximum count and total pixel count for a texture array.
inline constexpr int kMaxTextureArrayCount = 2048;
inline constexpr int kMaxTextureArrayPixels = 512 * 1024 * 1024;

// Maximum size of each side, and total texel count, for a volume texture.
inline constexpr int kMaxVolumeTextureSize = 2048;
inline constexpr int kMaxVolumeTextureTexels = 512 * 1024 * 1024;

// Maximum size in bytes of a storage buffer's values, which is the least any
// Vulkan device supports.
inline constexpr int kMaxStorageBufferBytes = 128 * 1024 * 1024;

}  // namespace gb

#endif  // GB_RENDER_RENDER_TYPES_H_
