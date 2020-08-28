#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject {
  mat4 kView;
  mat4 kProjection;
};

layout(set = 2, binding = 0) uniform ModelView {
  mat4 kModelMatrix;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUv;

void main() {
  gl_Position = kProjection * kView * kModelMatrix * vec4(inPosition, 1.0);
  outColor = inColor;
  outUv = inUv;
}
