// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_CHUNK_READER_H_
#define GB_FILE_CHUNK_READER_H_

#include <stdint.h>

#include "gb/file/chunk_types.h"
#include "gb/file/file_types.h"

namespace gb {

//==============================================================================
// ChunkReader
//==============================================================================

// This class returns a raw chunk for reading (optionally returned as part of
// ReadChunkFile).
//
// A ChunkReader is constructed via the Read factory method, which reads the
// chunk into memory directly, and validates the chunk header and size. Code
// that wants to access the data must then call GetChunkData. If the chunk data
// contains pointers to extra data, these will be in offset form initially and
// must be converted to pointers by calling ConvertToPtr on each member.
//
// Example:
//
// ```
//  // All chunks should have a unique chunk type.
//  inline static constexpr ChunkType kChunkTypeExample = {'X', 'M', 'P', 'L'};
//  inline static constexpr ChunkType kChunkTypeBar = {'B', 'A', 'R', 0};
//
//  struct Foo { int32_t x,y,z; }
//  struct Bar { float a,b,c; }
//
//  // Structure should be trivially copyable (can be initialized correctly via
//  // memcpy), as this is just read/written direction from/to files.
//  struct ExampleChunk {
//    // Use ChunkPtr<> to point to variable length data. Use const char for
//    // string data.
//    ChunkPtr<const char> name;
//
//    // Explicitly sized integer and float types are fine (and enumerations
//    // backed by corresponding explicitly sized integer types). Do not use
//    // bool, or non-sized integer types as they do not have a well defined
//    // size in bytes. gb/base/platform_requirements.cc ensures the expected
//    // sizing and alignment.
//    float value;
//    int32_t foo_count;
//
//    // ChunkPtr is 8-bytes and should be on an 8-byte boundary. In general,
//    // keep blocks of values in 8-byte increments, to ensure structure padding
//    // is what is expected. If necessary, add reserved fields as needed to do
//    // so.
//    ChunkPtr<const Foo> foos;
//  };
//
//  void HandleExampleChunk(ExampleChunk* example) {
//    // ... Do whatever processing is needed ...
//    std::free(example);
//  }
//
//  void HandleBarChunk(Bar* bars, int count) {
//    // ... Do whatever processing is needed ...
//    std::free(bars);
//  }
//
//  // Reads supported chunk from the file, ignoring all others.
//  void ReadChunk(File* file) {
//    auto chunk_reader = ChunkReader::Read(file);
//    if (!chunk_reader) {
//      // No chunk (pass in has_error to Read to detect errors).
//      return;
//    }
//    if (chunk_reader->GetType() == kChunkTypeExample) {
//      if (chunk_reader->GetVersion() > 1) { /* handle unexpected version */ }
//      auto* chunk = chunk_reader->GetChunkData<ExampleChunk>();
//      chunk_reader->ConvertToPtr(&chunk->name);
//      chunk_reader->ConvertToPtr(&chunk->foos);
//      HandleExampleChunk(chunk_reader->ReleaseChunkData<ExampleChunk>());
//    }
//    if (chunk_reader->GetType() == kChunkTypeBar) {
//      if (chunk_reader->GetVersion() > 1) { /* handle unexpected version */ }
//      HandleBarChunk(chunk_reader->ReleaseChunkData<Bar>(),
//                     chunk_reader->GetCount());
//    }
//    // Unhandled chunk will just get ignored.
//  }
// ```
class ChunkReader final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  ChunkReader(const ChunkReader&) = delete;
  ChunkReader(ChunkReader&& other);
  ChunkReader& operator=(const ChunkReader&) = delete;
  ChunkReader& operator=(ChunkReader&& other);
  ~ChunkReader();

  // Reads a chunk from the file, if there is a complete chunk.
  //
  // If this is the end of file or the chunk could not be read completely, this
  // will not return a chunk. If an error occurred (end of file is not
  // considered an error), then *has_error will be set to true if it is not
  // null.
  static std::optional<ChunkReader> Read(File* file, bool* has_error = nullptr);

  //----------------------------------------------------------------------------
  // Chunk header
  //----------------------------------------------------------------------------

  const ChunkType& GetType() const { return header_.type; }
  int32_t GetVersion() const { return header_.version; }
  int32_t GetSize() const { return header_.size; }
  int32_t GetCount() const { return header_.count; }

  //----------------------------------------------------------------------------
  // Chunk data
  //----------------------------------------------------------------------------

  // Returns a writable pointer to the chunk data cast to the specified type.
  //
  // Callers should call this if there are ChunkPtr members to patch, and call
  // ConvertToPtr on each. Note that all ownership of the chunk data and all
  // extra data is retained by ChunkReader until ReleaseChunkData is called.
  //
  // It is up to the caller to ensure the type is correct! If this is cast to
  // the wrong type, it is undefined behavior.
  template <typename Type>
  Type* GetChunkData();

  // Releases the the chunk data to the caller.
  //
  // Callers should call to acquire ownership of the underlying chunk data.
  // Further calls to GetChunkData or ReleaseChunkData will return null. Chunk
  // header data remains valid. It is the callers responsibility to call
  // std::free() on the returned chunk data. Note that the allocation may be
  // larger than sizeof(Type), which is where any data for ChunkPtr<> members is
  // stored.
  //
  // It is up to the caller to ensure the type is correct! If this is cast to
  // the wrong type, it is undefined behavior.
  template <typename Type>
  Type* ReleaseChunkData();

  //----------------------------------------------------------------------------
  // Extra data
  //----------------------------------------------------------------------------

  // Converts the ChunkPtr from offset representation to pointer representation.
  //
  // This must be a valid ChunkPtr within GetChunkData. If ReleaseChunkData was
  // called, this will set the pointer to null.
  template <typename Type>
  void ConvertToPtr(ChunkPtr<Type>* ptr);

 private:
  ChunkReader(const ChunkHeader& header, uint64_t* data)
      : header_(header), data_(data) {}

  ChunkHeader header_;
  uint64_t* data_ = nullptr;
};

//==============================================================================
// Chunk file helpers
//==============================================================================

// Helper to read the entirety of a chunk file.
//
// If "file_type" is not null, then the chunk file header file type will be
// copied into it. If "chunks" is null, then only the file header will be read,
// leaving any additional chunk reading to the caller (the file will be
// positioned immediately after the file header, in this case). If "chunks" is
// not null, then all chunks in the file will be read into the provided vector.
//
// Returns true if the chunk file header and all chunks (if requested) were
// successfully read.
bool ReadChunkFile(File* file, ChunkType* file_type,
                   std::vector<ChunkReader>* chunks);

//==============================================================================
// Template / inline implementation
//==============================================================================

inline ChunkReader::ChunkReader(ChunkReader&& other)
    : header_(other.header_), data_(std::exchange(other.data_, nullptr)) {}

template <typename Type>
inline Type* ChunkReader::GetChunkData() {
  static_assert(IsValidChunkType<Type>(),
                "Chunk type must be trivially copyable or void");
  return reinterpret_cast<Type*>(data_);
}

template <typename Type>
Type* ChunkReader::ReleaseChunkData() {
  static_assert(IsValidChunkType<Type>(),
                "Chunk type must be trivially copyable or void");
  return reinterpret_cast<Type*>(std::exchange(data_, nullptr));
}

template <typename Type>
inline void ChunkReader::ConvertToPtr(ChunkPtr<Type>* ptr) {
  if (data_ == nullptr || ptr->offset == 0) {
    ptr->ptr = nullptr;
  } else {
    ptr->ptr = reinterpret_cast<Type*>(data_ + (ptr->offset / 8));
  }
}

}  // namespace gb

#endif  // GB_FILE_CHUNK_READER_H_
