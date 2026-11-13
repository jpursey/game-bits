#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// A quad turned about its center, at a fixed depth, for the passes section.

layout(location = 0) in vec2 in_pos;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_coord;  // From (0, 0) to (1, 1) on the quad.
layout(location = 2) out vec4 out_params;

layout(set = 0, binding = 0) uniform ViewData {
  vec4 s_pixel_to_clip;  // A pixel position times xy, plus zw, is in clip space.
};

layout(set = 2, binding = 0) uniform PassQuadData {
  vec4 i_rect;    // xy is the center, and zw half the size, in pixels.
  vec4 i_color;
  vec4 i_params;  // x is the angle turned in radians, and y the depth.
};

void main() {
  vec2 corner = (in_pos * 2.0 - 1.0) * i_rect.zw;
  float c = cos(i_params.x);
  float s = sin(i_params.x);
  vec2 pixel =
      i_rect.xy + vec2(c * corner.x - s * corner.y, s * corner.x + c * corner.y);
  gl_Position =
      vec4(pixel * s_pixel_to_clip.xy + s_pixel_to_clip.zw, i_params.y, 1.0);
  out_color = i_color;
  out_coord = in_pos;
  out_params = i_params;
}
