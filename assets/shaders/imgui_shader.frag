#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

layout(set = 1, binding = 0) uniform sampler2D m_sampler;
layout(set = 1, binding = 1) uniform sampler2DArray m_sampler_array;
layout(set = 1, binding = 2) uniform SamplerInfo {
  float m_index;
};

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main()
{
  if (m_index < 0) {
    outColor = inColor * texture(m_sampler, inUv);
  } else {
    outColor = inColor * texture(m_sampler_array, vec3(inUv, m_index));
  }
}
