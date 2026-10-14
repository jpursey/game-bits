#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

layout(location = 0) in vec2 in_pos;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform SceneData {
  vec4 s_pixel_to_clip;  // xy scales a pixel position to clip space.
};

layout(set = 2, binding = 0) uniform InstanceData {
  vec4 i_rect;  // xy is the top left corner, zw the size, in pixels.
  vec4 i_color;
};

void main() {
  vec2 pixel = i_rect.xy + in_pos * i_rect.zw;
  gl_Position = vec4(pixel * s_pixel_to_clip.xy - 1.0, 0.0, 1.0);
  out_color = i_color;
}
