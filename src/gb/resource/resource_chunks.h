// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RESOURCE_RESOURCE_CHUNKS_H_
#define GB_RESOURCE_RESOURCE_CHUNKS_H_

#include "gb/file/chunk_types.h"
#include "gb/resource/resource_types.h"

namespace gb {

// Common resource chunk types
inline constexpr ChunkType kChunkTypeResourceLoad = {'G', 'B', 'R', 'L'};

// RenderSystem chunk types
inline constexpr ChunkType kChunkTypeMaterial = {'G', 'B', 'M', 'A'};
inline constexpr ChunkType kChunkTypeMaterialType = {'G', 'B', 'M', 'T'};
inline constexpr ChunkType kChunkTypeMesh = {'G', 'B', 'M', 'E'};
inline constexpr ChunkType kChunkTypeShader = {'G', 'B', 'S', 'H'};
inline constexpr ChunkType kChunkTypeTexture = {'G', 'B', 'T', 'X'};
inline constexpr ChunkType kChunkTypeTextureArray = {'G', 'B', 'T', 'A'};

// The ResourceLoadChunk defines a resource that a chunk file depends on and
// must be loaded first with the specified name.
struct ResourceLoadChunk {
  ChunkPtr<const char> type;
  ChunkPtr<const char> name;
  ResourceId id;
};

}  // namespace gb

#endif  // GB_RESOURCE_RESOURCE_CHUNKS_H_
