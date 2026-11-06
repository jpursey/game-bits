#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

layout(location = 0) in vec3 in_coord;

layout(location = 0) out vec4 out_color;

// Matches QuadColor in render_check.h.
struct QuadColor {
  vec4 color;
};

// None of these are ever set, so each draws with its default.
layout(set = 1, binding = 0) uniform sampler2D tex;
layout(set = 1, binding = 1) uniform sampler2DArray tex_array;
layout(set = 1, binding = 2) uniform sampler3D volume;
layout(std430, set = 1, binding = 3) readonly buffer QuadColors {
  QuadColor colors[];
};

// The default textures are opaque white and the default storage buffer is
// zeros, so the quad is white.
void main() {
  out_color = texture(tex, in_coord.xy) *
                  texture(tex_array, vec3(in_coord.xy, 0.0)) *
                  texture(volume, in_coord) -
              colors[0].color;
}
