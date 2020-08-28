#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D kSampler;

void main() {
  outColor = vec4(inColor * texture(kSampler, inUv).rgb, 1.0);
}
