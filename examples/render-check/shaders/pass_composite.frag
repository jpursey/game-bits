#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// Draws the world pass's color, and writes its depth, so what is drawn after
// it in the frame is depth tested against the world.

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform ViewData {
  vec4 s_pixel_to_clip;  // xy is two over the frame's size.
};

layout(set = 1, binding = 0) uniform sampler2D world_color;
layout(set = 1, binding = 1) uniform sampler2D world_depth;

void main() {
  // The world's targets are the frame's size.
  vec2 uv = gl_FragCoord.xy * s_pixel_to_clip.xy * 0.5;
  out_color = texture(world_color, uv);
  gl_FragDepth = texture(world_depth, uv).r;
}
