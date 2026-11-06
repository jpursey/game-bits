#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// Vertices are in pixels. Each draw's first instance is the index of its color
// in the storage buffer, which storage.frag reads.

layout(location = 0) in vec2 in_pos;

layout(location = 0) flat out int out_index;

layout(set = 0, binding = 0) uniform SceneData {
  vec4 s_pixel_to_clip;  // xy scales a pixel position to clip space.
};

void main() {
  gl_Position = vec4(in_pos * s_pixel_to_clip.xy - 1.0, 0.0, 1.0);
  out_index = gl_InstanceIndex;
}
