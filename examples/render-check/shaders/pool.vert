#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// Vertices are in pixels, moved by the instance's offset. Each draw's first
// instance is the index of its color in the storage buffer, which storage.frag
// reads.

layout(location = 0) in vec2 in_pos;

layout(location = 0) flat out int out_index;

layout(set = 0, binding = 0) uniform ViewData {
  vec4 s_pixel_to_clip;  // xy scales a pixel position to clip space.
};

layout(set = 2, binding = 0) uniform InstanceData {
  vec4 i_offset;  // xy is added to each vertex, in pixels.
  vec4 i_unused;
};

void main() {
  vec2 pixel = in_pos + i_offset.xy;
  gl_Position = vec4(pixel * s_pixel_to_clip.xy - 1.0, 0.0, 1.0);
  out_index = gl_InstanceIndex;
}
