#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D kSampler;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main()
{
  outColor = inColor * texture(kSampler, inUv);
}
