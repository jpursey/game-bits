// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_FORMAT_H_
#define GB_RENDER_WEBGPU_WEBGPU_FORMAT_H_

#include <cstdint>

#include "absl/types/span.h"
#include "gb/render/render_types.h"
#include "webgpu/webgpu_cpp.h"

namespace gb {

//==============================================================================
// Formats
//==============================================================================

// Returns the WebGPU format the GPU holds texels of `format` in. kRgba4 is held
// as RGBA8Unorm, expanded by ExpandRgba4(), as WebGPU has no 4-bit format, and
// kDepth as Depth32Float.
wgpu::TextureFormat GetWebGpuFormat(TextureFormat format);

// Returns the size in bytes of one texel of `format` as the GPU holds it: 4 for
// kRgba4, and otherwise GetTexelSize().
int GetWebGpuTexelSize(TextureFormat format);

// Returns the size in bytes of a texture's texels as the GPU holds them.
inline int64_t GetWebGpuTextureSizeInBytes(TextureFormat format, int width,
                                           int height, int depth = 1) {
  return int64_t{width} * height * depth * GetWebGpuTexelSize(format);
}

// Expands kRgba4 texels to the RGBA8Unorm texels the GPU holds: each 4-bit
// channel `v` becomes `v * 17`, so 0 stays 0 and 15 becomes 255, and a shader
// reads the same value it would from a 4-bit channel. `out` holds 4 bytes for
// each texel.
void ExpandRgba4(absl::Span<const uint16_t> texels, uint8_t* out);

// Returns `value` rounded up to a multiple of `alignment`, as WebGPU's copies
// and writes need.
inline constexpr int64_t AlignUp(int64_t value, int64_t alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

//==============================================================================
// Bindings
//==============================================================================

// The highest binding index the backend takes. WebGPU numbers a bind group's
// bindings below 1000, and a texture binding takes two of them.
inline constexpr int kMaxWebGpuBindingIndex = 499;

// What a WGSL binding holds, which decides its number.
enum class WgslBindingKind {
  kResource,  // A texture, constants, or a storage buffer.
  kSampler,   // The sampler of a texture binding.
};

// Returns the WGSL @binding number of a gb_render binding index: 2 * index for
// a resource, and 2 * index + 1 for a texture's sampler, as gb_embed_shader
// numbers them in a web build. A WGSL shader written by hand numbers its
// bindings the same way.
inline constexpr int GetWgslBinding(int index, WgslBindingKind kind) {
  return 2 * index + (kind == WgslBindingKind::kSampler ? 1 : 0);
}

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_FORMAT_H_
