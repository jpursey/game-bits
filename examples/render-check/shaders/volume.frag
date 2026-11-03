#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

layout(location = 0) in vec3 in_coord;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler3D volume;

void main() { out_color = texture(volume, in_coord); }
