// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_CHUNK_TYPES_H_
#define GB_FILE_CHUNK_TYPES_H_

#include <stdint.h>

#include <string_view>
#include <type_traits>

#include "absl/hash/hash.h"

namespace gb {

//==============================================================================
// Type hepers
//==============================================================================

// Returns true if the type is a valid type for accessing chunk data.
template <typename Type>
constexpr bool IsValidChunkType() {
  return std::conditional_t<
      // "void" is a valid type for accessing untyped raw data.
      std::is_same_v<std::remove_const_t<Type>, void>, std::true_type,
      // If it is types, it must be trivially copyable.
      std::conditional_t<
          std::is_trivially_copyable_v<std::remove_const_t<Type>>,
          std::true_type, std::false_type>>::value;
}

// Returns the minimum space needed for a chunk structure.
template <typename Type>
constexpr int32_t GetChunkTypeSize() {
  return static_cast<int32_t>(sizeof(Type));
}
template <>
constexpr int32_t GetChunkTypeSize<void>() {
  return 1;
}
template <>
constexpr int32_t GetChunkTypeSize<const void>() {
  return 1;
}

//==============================================================================
// ChunkType
//==============================================================================

// A chunk type is a unique 4-character code that identifies a chunk in a chunk
// file.
//
// Chunk Types can be compared for equality against itself or other string-like
// types, and may also be used as keys in maps and sets (absl or std).
//
// Note: All Game Bits chunk codes start with the letters 'G' 'B'. Applications
// or other libraries using Game Bits chunk files should avoid that letter
// combination prefix to maximize compatibility. Game Bits uses chunk files to
// serialize most all binary resource formats.
struct ChunkType {
  char code[4];

  std::string_view ToString() const;
};
static_assert(sizeof(ChunkType) == 4);
static_assert(std::is_trivially_copyable_v<ChunkType>);

inline constexpr ChunkType kChunkTypeNone = {0, 0, 0, 0};
inline constexpr ChunkType kChunkTypeFile = {'G', 'B', 'F', 'I'};

//==============================================================================
// ChunkHeader
//==============================================================================

// Defines the 16-byte header for all chunks.
//
// Chunk files contain one or more chunks, and are generally associated with a
// specific chunk type (for instance, corresponding to a resource being loaded).
// There are three types of chunks:
// - File chunk: This is always the first chunk in a file, and it defines the
//   chunk file version, as well as the primary chunk type this file is for. The
//   "file" member specifies the file's primary chunk type.
// - Single chunk: This is a chunk that is defined in its entirety by a single
//   structured block of data. The "file" and "count" fields are unused (and
//   will be zero).
// - List chunk: This is a chunk that consists of a list of chunk entries. The
//   "count" field indicates the number of entries in the chunk. Note that
//   additional data may also be stored after the entries in the chunk.
//
// Chunk files are always 8-byte aligned to support direct loading into memory
// and then patching pointers, etc.
//
// Well defined chunk files are laid out as follows:
//    Chunk "GBFI"    <-- The first chunk of a complete file is always "GBFI"
//      size: 0       <-- The file header chunk must always be empty.
//      version: 1    <-- All chunk files are version 1 currently.
//      file: "????"  <-- Optional specification of the primary chunk.
//    Chunk "XXXX"
//    Chunk "????"    <-- Primary chunk.
//    Chunk "YYYY"
//    Chunk "YYYY"    <-- Multiple chunks of the same type are allowed.
//    Chunk "ZZZZ"
//    ... and so on, until end-of-file ...
struct ChunkHeader {
  ChunkType type;   // Unique chunk type. All game bits chunks start with "GB".
  int32_t size;     // Size in bytes of the chunk, not including the header.
                    // This is always a multiple 8.
  int32_t version;  // Version of the chunk or chunk file. This determines the
                    // layout of the chunk, or in the case of the file chunk,
                    // the layout of the file itself.
  union {
    int32_t count;   // For non-file chunks, this indicates number of entries
                     // following the chunk header.
    ChunkType file;  // For file chunks, this indicates the primary chunk type
                     // this file is for. If this is a generic file, this is
                     // kChunkTypeNone.
  };
};
static_assert(sizeof(ChunkHeader) == 16);
static_assert(std::is_trivially_copyable_v<ChunkHeader>);

//==============================================================================
// ChunkPtr
//==============================================================================

// A ChunkPtr represents a pointer to additional data in the chunk.
//
// When chunk is in its file format, the "offset" form is used. When it is used
// in memory, the "ptr" form may be used (manually convertible when reading with
// ChunkReader).
template <typename Type>
union ChunkPtr {
  static_assert(IsValidChunkType<Type>(),
                "Chunk ptr must void or a trivially copyable type");

  int64_t offset;  // In file form, pointers must be stored as offsets,
                   // converted by calling AddData or AddString in ChunkWriter.
  Type* ptr;       // After being read from a file, the value may be converted
                   // from the offset by calling ConvertToPtr using the
                   // ChunkReader class.
};
static_assert(sizeof(ChunkPtr<void>) == 8);
static_assert(std::is_trivially_copyable_v<ChunkPtr<void>>);

//==============================================================================
// Template / inline implementation
//==============================================================================

inline std::string_view ChunkType::ToString() const {
  return std::string_view(code, code[3] == 0 ? std::strlen(code) : 4);
}

inline bool operator==(const ChunkType& a, const ChunkType& b) {
  return a.ToString() == b.ToString();
}
inline bool operator!=(const ChunkType& a, const ChunkType& b) {
  return !(a == b);
}

inline bool operator==(const ChunkType& type, std::string_view str) {
  return type.ToString() == str;
}
inline bool operator==(std::string_view str, const ChunkType& type) {
  return type == str;
}
inline bool operator!=(const ChunkType& type, std::string_view str) {
  return !(type == str);
}
inline bool operator!=(std::string_view str, const ChunkType& type) {
  return !(type == str);
}

inline bool operator<(const ChunkType& a, const ChunkType& b) {
  return a.ToString() < b.ToString();
}
inline bool operator>(const ChunkType& a, const ChunkType& b) { return b < a; }
inline bool operator<=(const ChunkType& a, const ChunkType& b) {
  return !(b < a);
}
inline bool operator>=(const ChunkType& a, const ChunkType& b) {
  return !(a < b);
}

template <typename State>
State AbslHashValue(State state, const ChunkType& type) {
  return State::combine(std::move(state), type.ToString());
}

}  // namespace gb

namespace std {

template <>
struct hash<gb::ChunkType> {
  size_t operator()(const gb::ChunkType& type) {
    return std::hash<std::string_view>{}(type.ToString());
  }
};

}  // namespace std

#endif  // GB_FILE_CHUNK_TYPES_H_
