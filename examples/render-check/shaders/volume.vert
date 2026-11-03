#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

layout(location = 0) in vec2 in_pos;

layout(location = 0) out vec3 out_coord;

layout(set = 0, binding = 0) uniform SceneData {
  vec4 s_pixel_to_clip;  // xy scales a pixel position to clip space.
};

layout(set = 2, binding = 0) uniform InstanceData {
  vec4 i_rect;   // xy is the top left corner, zw the size, in pixels.
  vec4 i_depth;  // x is the depth to sample the volume at, from 0 to 1.
};

void main() {
  vec2 pixel = i_rect.xy + in_pos * i_rect.zw;
  gl_Position = vec4(pixel * s_pixel_to_clip.xy - 1.0, 0.0, 1.0);
  out_coord = vec3(in_pos, i_depth.x);
}
