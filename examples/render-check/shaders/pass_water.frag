#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// Water over the world pass's color, offset by moving waves, and tinted more
// the deeper the world is below it.

layout(location = 0) in vec4 in_color;   // The water's tint.
layout(location = 2) in vec4 in_params;  // z is the phase of the waves.

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform ViewData {
  vec4 s_pixel_to_clip;  // xy is two over the frame's size.
};

layout(set = 1, binding = 0) uniform sampler2D world_color;
layout(set = 1, binding = 1) uniform sampler2D world_depth;

void main() {
  // The world's targets are the frame's size.
  vec2 pixel_to_uv = s_pixel_to_clip.xy * 0.5;
  vec2 pixel = gl_FragCoord.xy;
  vec2 offset = vec2(sin(pixel.y * 0.3 + in_params.z),
                     cos(pixel.x * 0.05 + in_params.z)) *
                6.0;
  vec3 below = texture(world_color, (pixel + offset) * pixel_to_uv).rgb;
  float below_depth = texture(world_depth, pixel * pixel_to_uv).r;
  float deep = clamp((below_depth - gl_FragCoord.z) * 2.0, 0.0, 1.0);
  out_color = vec4(mix(below, in_color.rgb, 0.2 + 0.6 * deep), 1.0);
}
