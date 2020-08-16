// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RESOURCE_FILE_RESOURCE_CHUNKS_H_
#define GB_RESOURCE_FILE_RESOURCE_CHUNKS_H_

#include "gb/file/chunk_types.h"
#include "gb/resource/resource_types.h"

namespace gb {

inline constexpr ChunkType kChunkTypeResourceLoad = {'G', 'B', 'R', 'L'};

// The ResourceLoadChunk defines a resource that a chunk file depends on and
// must be loaded first with the specified name.
struct ResourceLoadChunk {
  ChunkPtr<const char> type;
  ChunkPtr<const char> name;
  ResourceId id;
};

}  // namespace gb

#endif  // GB_RESOURCE_FILE_RESOURCE_CHUNKS_H_
