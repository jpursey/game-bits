// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMGUI_IMGUI_SHADERS_H_
#define GB_IMGUI_IMGUI_SHADERS_H_

#include <cstdint>

#include "absl/types/span.h"

namespace gb::internal {

//==============================================================================
// The code of ImGuiInstance's shaders, for RenderSystem::CreateShaderCode(),
// in the form the platform's render backend takes: SPIR-V for Vulkan, or WGSL
// in a web build. Their source is assets/shaders/imgui_shader.vert and .frag.
//==============================================================================

absl::Span<const uint8_t> GetImGuiVertexShaderCode();
absl::Span<const uint8_t> GetImGuiFragmentShaderCode();

}  // namespace gb::internal

#endif  // GB_IMGUI_IMGUI_SHADERS_H_
