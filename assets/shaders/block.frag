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

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform SceneLightData {
  vec4 s_ambient;        // w is intensity
  vec4 s_sun_color;      // w is intensity
  vec3 s_sun_direction;
};

layout(set = 1, binding = 0) uniform sampler2D m_sampler;

vec3 GetDirectionalLight(vec3 normal, vec4 color, vec3 direction) {
  vec3 to_light = normalize(-direction);
  float contribution = clamp(dot(normal, to_light), 0.0, 1.0);
  return contribution * color.w * color.rgb;
}

void main() {
  vec3 normal = normalize(in_normal);
  vec3 light_mod = GetDirectionalLight(normal, s_sun_color, s_sun_direction);
  vec3 light_add = s_ambient.w * s_ambient.rgb;
  vec4 mat_color = texture(m_sampler, in_uv);
  out_color = vec4(light_add + light_mod * mat_color.xyz, mat_color.w);
}
