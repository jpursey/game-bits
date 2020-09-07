#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

layout(set = 0, binding = 0) uniform SceneConstants {
  vec2 kScale;
  vec2 kOffset;
};

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUv;

void main() {
  gl_Position = vec4(inPosition * kScale + kOffset, 0, 1);
  outColor = inColor;
  outUv = inUv;
}
