#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// Mist in the world pass's color, thicker the further the world is behind it,
// drawn in a pass that depth tests against the world's depth while sampling it.
// It is premultiplied.

layout(location = 0) in vec4 in_color;  // The mist's color.

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform ViewData {
  vec4 s_pixel_to_clip;  // xy is two over the frame's size.
};

layout(set = 1, binding = 1) uniform sampler2D world_depth;

void main() {
  // The world's targets are the frame's size.
  vec2 uv = gl_FragCoord.xy * s_pixel_to_clip.xy * 0.5;
  float behind = texture(world_depth, uv).r - gl_FragCoord.z;
  float alpha = clamp(0.1 + behind * 1.8, 0.0, 0.9);
  out_color = vec4(in_color.rgb * alpha, alpha);
}
