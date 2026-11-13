#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// Shows a region of a depth target as gray, from black at the near plane to
// white at the far plane.

layout(location = 0) in vec4 in_color;  // The region: xy is its top left, and
                                        // zw its size, in texture coordinates.
layout(location = 1) in vec2 in_coord;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D depth;

void main() {
  float d = texture(depth, in_color.xy + in_coord * in_color.zw).r;
  out_color = vec4(d, d, d, 1.0);
}
