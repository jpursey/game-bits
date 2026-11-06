#version 460
#extension GL_ARB_separate_shader_objects : enable

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// The vertex shader for chunks drawn from the world's mesh pool. Each chunk's
// draw has its own first instance, which is the index of its position in the
// scene's storage buffer.

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_uv;
layout(location = 3) in vec4 in_color;

layout(location = 0) out vec3 out_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec3 out_uv;
layout(location = 3) out vec4 out_color;

layout(set = 0, binding = 0) uniform SceneData {
  mat4 s_view_projection;
};

struct ChunkPosition {
  vec4 offset;  // xyz is added to each vertex.
};

layout(set = 0, binding = 2) readonly buffer ChunkPositions {
  ChunkPosition s_chunks[];
};

void main() {
  out_pos = in_pos + s_chunks[gl_InstanceIndex].offset.xyz;
  out_normal = in_normal;
  out_color = in_color;
  out_uv = in_uv;
  gl_Position = s_view_projection * vec4(out_pos, 1.0);
}
