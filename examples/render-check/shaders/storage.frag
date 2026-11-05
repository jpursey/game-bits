#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

layout(location = 0) flat in int in_index;

layout(location = 0) out vec4 out_color;

// Matches QuadColor in render_check.h.
struct QuadColor {
  vec4 color;
};

layout(std430, set = 1, binding = 0) readonly buffer QuadColors {
  QuadColor colors[];
};

void main() { out_color = colors[in_index].color; }
