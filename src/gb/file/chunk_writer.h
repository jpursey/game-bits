// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_CHUNK_WRITER_H_
#define GB_FILE_CHUNK_WRITER_H_

#include <stdint.h>

#include <string_view>
#include <tuple>
#include <vector>

#include "absl/types/span.h"
#include "gb/file/chunk_types.h"
#include "gb/file/file_types.h"

namespace gb {

//==============================================================================
// ChunkWriter
//==============================================================================

// This class builds a chunk for writing via WriteChunkFile.
//
// A ChunkWriter is constructed via the New factory method, which defines the
// core memory chunk type or chunk entry type (if a non-zero count is
// specified). Code that writes must then call use GetChunkData to initialize
// the chunk. If the chunk data contains pointers to extra data, then these can
// be added and converted to a write-comaptible format by calling AddData or
// AddString.
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
//  // Writes an ExampleChunk as a single chunk (not a list).
//  ChunkWriter WriteExampleChunk(std::string_view name, float value,
//                                absl::Span<const Foo> foos) {
//    auto chunk_writer = ChunkWriter::New<ExampleChunk>(kChunkTypeExample, 1);
//    auto* chunk = chunk_writer->GetChunkData<ExampleChunk>();
//    chunk->name = chunk_writer.AddString(name);
//    chunk->value = value;
//    chunk->foo_count = static_cast<int32_t>(foos.size());
//    chunk->foos = chunk_writer.AddPtr(foos.data(), chunk.foo_count);
//    return chunk_writer;
//  }
//
//  // Writes a list chunk of Bars (where bar is trivial with no ChunkPtr
//  // members).
//  ChunkWriter WriteBarChunk(absl::Span<const Bar> bars) {
//    auto chunk_writer = ChunkWriter::New<Bar>(
//        kChunkTypeBar, 1, static_cast<int32_t>(bars.size()));
//    std::memcpy(chunk_writer->GetChunkData(), bars.data(), bars.size());
//    return chunk_writer;
//  }
// ```
class ChunkWriter final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  ChunkWriter(const ChunkWriter&) = delete;
  ChunkWriter(ChunkWriter&&);
  ChunkWriter& operator=(const ChunkWriter&) = delete;
  ChunkWriter& operator=(ChunkWriter&&);
  ~ChunkWriter();

  // Instantiates a new ChunkWriter with space for one Type of chunk data.
  //
  // Type must be trivially copyable. All chunk data will be zero-filled
  // initially. Note that the total chunk size may be larger to ensure 8-byte
  // total alignment for the chunk.
  //
  // The version passed in can be any value >= 1, and is intended for versioning
  // the structure of the data in the file so it can be handled correctly on
  // read.
  template <typename Type>
  static ChunkWriter New(const ChunkType& type, int32_t version);

  // Instantiates a new ChunkWriter with space for "count" chunk entries of
  // Type.
  //
  // Type must be trivially copyable. All chunk data will be zero-filled
  // initially. Note that the total chunk size may be larger to ensure 8-byte
  // total alignment for the chunk.
  //
  // The version passed in can be any value >= 1, and is intended for versioning
  // the structure of the data in the file so it can be handled correctly on
  // read.
  template <typename Type>
  static ChunkWriter New(const ChunkType& type, int32_t version, int32_t count);

  // Instantiates a new ChunkWriter with an explicit pre-allocated data buffer.
  //
  // The data is considered "used" as the primary chunk data, and will be
  // returned by GetChunkData(). The data is not owned however, and so must
  // remain valid longer than whichever ChunkWriter references it (passed via
  // move semantics).
  static ChunkWriter New(const ChunkType& type, int32_t version, void* data,
                         int32_t data_size);

  //----------------------------------------------------------------------------
  // Chunk header
  //----------------------------------------------------------------------------

  const ChunkType& GetType() const { return header_.type; }
  int32_t GetVersion() const { return header_.version; }
  int32_t GetSize() const { return header_.size; }
  int32_t GetCount() const { return header_.count; }

  //----------------------------------------------------------------------------
  // Chunk data access
  //----------------------------------------------------------------------------

  // Retrieves the typed chunk data (or array of chunk entries) reserved by New.
  //
  // Type must be the same as what was passed to New.
  template <typename Type>
  Type* GetChunkData();

  //----------------------------------------------------------------------------
  // Extra storage
  //----------------------------------------------------------------------------

  // Adds data of the specified type to the chunk returning an
  // offset-initialized ChunkPtr to the data.
  //
  // Type must be trivially copyable. It always will be stored at an 8-byte
  // aligned location always. "count" indicates the size of the array or the
  // number of bytes if Type is void.
  //
  // Passing null for data, will always result in a null ChunkPtr offset.
  // "count" is ignored in this case.
  template <typename Type>
  ChunkPtr<Type> AddData(const Type* data, int32_t count = 1);

  // Adds array-like data of the specified type to the chunk returning an
  // offset-initialized ChunkPtr to the data.
  //
  // Type must be void or trivially copyable. It will always be stored at an
  // 8-byte aligned location always.
  //
  // Passing in an empty span for data, will always result in a null ChunkPtr
  // offset.
  template <typename Type>
  ChunkPtr<Type> AddData(absl::Span<const Type> data);

  // Adds a string to the chunk returning an offset-initialized ChunkPtr to the
  // string.
  ChunkPtr<const char> AddString(std::string_view str);

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Writes this chunk to a file.
  //
  // Returns false if the chunk could not be completely written. It is possible
  // the chunk may be partially written in this case (the file may be modified).
  bool Write(File* file) const;

 private:
  ChunkWriter(const ChunkType& type, int32_t version, int32_t count,
              int32_t item_size, void* chunk_data = nullptr);

  std::tuple<int64_t, void*> ReserveExtra(int64_t total_size);

  ChunkHeader header_;
  void* chunk_buffer_ = nullptr;
  int32_t chunk_buffer_size_ = 0;
  bool owns_chunk_buffer_ = false;
  std::vector<uint64_t> extra_buffer_;
};

//==============================================================================
// Chunk file helpers
//==============================================================================

// Helper to write a complete chunk file, including chunk file header.
//
// If "chunks" are empty, this will only write out the chunk header, leaving any
// additional chunk writing to the caller. The file will be positioned after the
// last requested chunk is written (or after the file header, if no chunks were
// specified).
//
// Returns true if the chunk file header and all chunks were successfully
// written.
bool WriteChunkFile(File* file, const ChunkType& file_type,
                    absl::Span<const ChunkWriter> chunks);

//==============================================================================
// Template / inline implementation
//==============================================================================

template <typename Type>
inline ChunkWriter ChunkWriter::New(const ChunkType& type, int32_t version) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Chunk type must be trivially copyable (void is not allowed)");
  return ChunkWriter(type, version, 1, GetChunkTypeSize<Type>());
}

template <typename Type>
inline ChunkWriter ChunkWriter::New(const ChunkType& type, int32_t version,
                                    int32_t count) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Chunk type must be trivially copyable (void is not allowed)");
  return ChunkWriter(type, version, count, GetChunkTypeSize<Type>());
}

inline ChunkWriter ChunkWriter::New(const ChunkType& type, int32_t version,
                                    void* data, int32_t data_size) {
  return ChunkWriter(type, version, 1, data_size, data);
}

template <typename Type>
inline Type* ChunkWriter::GetChunkData() {
  static_assert(IsValidChunkType<Type>(),
                "Chunk type must be trivially copyable or void");
  if (chunk_buffer_size_ < GetChunkTypeSize<Type>()) {
    return nullptr;
  }
  return static_cast<Type*>(chunk_buffer_);
}

template <typename Type>
ChunkPtr<Type> ChunkWriter::AddData(const Type* data, int32_t count) {
  static_assert(IsValidChunkType<Type>(),
                "Chunk type must be trivially copyable or void");
  ChunkPtr<Type> chunk_ptr;
  if (data == nullptr || count == 0) {
    chunk_ptr.offset = 0;
    return chunk_ptr;
  }
  auto [offset, ptr] = ReserveExtra(GetChunkTypeSize<Type>() * count);
  std::memcpy(ptr, data, GetChunkTypeSize<Type>() * count);
  chunk_ptr.offset = offset;
  return chunk_ptr;
}

template <typename Type>
inline ChunkPtr<Type> ChunkWriter::AddData(absl::Span<const Type> data) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Chunk type must be trivially copyable (void is not allowed)");
  return AddData<Type>(data.data(), static_cast<int32_t>(data.size()));
}

inline ChunkPtr<const char> ChunkWriter::AddString(std::string_view str) {
  auto [offset, ptr] = ReserveExtra(str.size() + 1);
  std::memcpy(ptr, str.data(), str.size());
  ChunkPtr<const char> chunk_ptr;
  chunk_ptr.offset = offset;
  return chunk_ptr;
}

}  // namespace gb

#endif  // GB_FILE_CHUNK_WRITER_H_
