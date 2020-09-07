#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_color;

layout(location = 0) out vec4 out_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec4 out_color;

layout(set = 0, binding = 0) uniform SceneData {
  mat4 s_view_projection;
};

layout(set = 2, binding = 0) uniform InstanceData {
  mat4 i_model;
};

void main() {
  out_pos = i_model * vec4(in_pos, 1.0);
  out_normal = mat3(i_model) * in_normal;
  out_color = in_color;
  out_uv = in_uv;
  gl_Position = s_view_projection * out_pos;
}
