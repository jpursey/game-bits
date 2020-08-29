// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_RESOURCE_CHUNKS_H_
#define GB_RENDER_RENDER_RESOURCE_CHUNKS_H_

#include "gb/file/chunk_types.h"
#include "gb/render/render_types.h"

namespace gb {

// Resource chunk types
inline constexpr ChunkType kChunkTypeBinding = {'G', 'B', 'B', 'N'};
inline constexpr ChunkType kChunkTypeInstanceBindingData = {'G', 'B', 'B', 'I'};
inline constexpr ChunkType kChunkTypeMaterial = {'G', 'B', 'M', 'A'};
inline constexpr ChunkType kChunkTypeMaterialBindingData = {'G', 'B', 'B', 'M'};
inline constexpr ChunkType kChunkTypeMaterialType = {'G', 'B', 'M', 'T'};
inline constexpr ChunkType kChunkTypeMesh = {'G', 'B', 'M', 'E'};
inline constexpr ChunkType kChunkTypeShader = {'G', 'B', 'S', 'H'};
inline constexpr ChunkType kChunkTypeTexture = {'G', 'B', 'T', 'X'};

// Entry for binding chunk.
struct BindingChunk {
  uint8_t shaders;      // ShaderTypes
  uint8_t set;          // BindingSet
  uint16_t index;       // Binding index
  uint16_t type;        // BindingType
  uint16_t volatility;  // DataVolatility
  ChunkPtr<const char> constants_name;
};

// Entry for scene, material, or instance binding data chunk.
struct BindingDataChunk {
  int32_t type;   // BindingType
  int32_t index;  // Binding index
  union {
    ResourceId texture_id;
    ChunkPtr<const void> constants_data;
  };
};

// Chunk for mesh resource.
//
// Mesh resource files are structured as follows:
//   Chunk "GBFI"  -- File header of file type "GBME".
//   Chunk "GBRL"  -- Optional: Specifies dependent resource names.
//   Chunk "GBME"  -- Mesh data.
struct MeshChunk {
  ResourceId id;
  ResourceId material_id;
  int32_t volatility;  // DataVolatility
  int32_t index_count;
  int32_t vertex_count;
  int32_t vertex_size;
  ChunkPtr<const void> vertices;
  ChunkPtr<const uint16_t> indices;
};

// Chunk for material resource.
//
// Material resource files are structured as follows:
//   Chunk "GBFI"  -- File header of file type "GBMA".
//   Chunk "GBRL"  -- Optional: Specifies dependent resource names.
//   Chunk "GBBN"  -- Binding data for the material (must match material type).
//   Chunk "GBBM"  -- Material binding data (must match bindings).
//   Chunk "GBBI"  -- Default instance binding data (must match bindings).
//   Chunk "GBMA"  -- Material data.
struct MaterialChunk {
  ResourceId id;
  ResourceId material_type_id;
};

// Chunk for material type resource.
//
// Material type resource files are structured as follows:
//   Chunk "GBFI"  -- File header of file type "GBMT".
//   Chunk "GBRL"  -- Optional: Specifies dependent resource names.
//   Chunk "GBBN"  -- Binding data for the material type.
//   Chunk "GBBM"  -- Default material binding data (must match bindings).
//   Chunk "GBBI"  -- Default instance binding data (must match bindings).
//   Chunk "GBMT"  -- Material type data.
struct MaterialTypeChunk {
  ResourceId id;
  ResourceId vertex_shader_id;
  ResourceId fragment_shader_id;
  ChunkPtr<const char> scene_type_name;
  ChunkPtr<const char> vertex_type_name;
  uint8_t cull_mode;
  uint8_t depth_mode;
};

// Entry for shader parameters in a shader chunk
struct ShaderParamEntry {
  int32_t value;
  int32_t location;
};

// Chunk for shader resource.
//
// Shader resource files are structured as follows:
//   Chunk "GBFI"  -- File header of file type "GBSH".
//   Chunk "GBBN"  -- Bindings for shader.
//   Chunk "GBSH"  -- Shader data.
struct ShaderChunk {
  ResourceId id;
  int32_t type;
  int32_t input_count;
  int32_t output_count;
  int32_t code_size;
  ChunkPtr<const ShaderParamEntry> inputs;
  ChunkPtr<const ShaderParamEntry> outputs;
  ChunkPtr<const void> code;
};

// Chunk for texture resource.
//
// Texture resource files are structured as follows:
//   Chunk "GBFI"  -- File header of file type "GBTX".
//   Chunk "GBTX"  -- Texture data.
struct TextureChunk {
  ResourceId id;
  int32_t volatility;  // DataVolatility
  uint16_t width;
  uint16_t height;
  ChunkPtr<const Pixel> pixels;
};

}  // namespace gb

#endif  // GB_RENDER_RENDER_RESOURCE_CHUNKS_H_
