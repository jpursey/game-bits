// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/chunk_reader.h"

#include "absl/log/log.h"
#include "gb/file/file.h"

namespace gb {

ChunkReader& ChunkReader::operator=(ChunkReader&& other) {
  if (&other == this) {
    return *this;
  }
  header_ = other.header_;
  data_ = std::exchange(other.data_, nullptr);
  return *this;
}

ChunkReader::~ChunkReader() { std::free(data_); }

std::optional<ChunkReader> ChunkReader::Read(File* file, bool* has_error) {
  ChunkHeader chunk_header = {};
  int64_t read_size =
      file->Read(static_cast<void*>(&chunk_header), sizeof(chunk_header));
  if (read_size != sizeof(chunk_header)) {
    if (has_error != nullptr) {
      *has_error = (read_size != 0);
    }
    return {};
  }

  if (chunk_header.version <= 0 || chunk_header.size < 0 ||
      chunk_header.size % 8 != 0 || chunk_header.count < 0 ||
      chunk_header.count > chunk_header.size) {
    LOG(ERROR) << "Corrupt chunk in chunk file";
    if (has_error != nullptr) {
      *has_error = true;
    }
    return {};
  }

  uint64_t* data = nullptr;
  const int64_t chunk_size = chunk_header.size / 8;
  if (chunk_size > 0) {
    data = static_cast<uint64_t*>(std::malloc(chunk_header.size));
    if (file->Read(data, chunk_size) != chunk_size) {
      LOG(ERROR) << "Chunk " << chunk_header.type.ToString()
                 << " is not complete";
      if (has_error != nullptr) {
        *has_error = true;
      }
      std::free(data);
      return {};
    }
  }

  if (has_error != nullptr) {
    *has_error = false;
  }
  return ChunkReader(chunk_header, data);
}

bool ReadChunkFile(File* file, ChunkType* file_type,
                   std::vector<ChunkReader>* chunks) {
  ChunkHeader file_header = {};
  if (file->Read(&file_header) != 1) {
    LOG(ERROR) << "Failed to read chunk file header";
    return false;
  }
  if (file_header.type != kChunkTypeFile) {
    LOG(ERROR) << "File is not a chunk file";
    return false;
  }
  if (file_header.version < 0 || file_header.size != 0) {
    LOG(ERROR) << "Corrupt chunk file";
    return false;
  }
  if (file_header.version > 1) {
    LOG(ERROR) << "Unsupported chunk file version: " << file_header.version;
    return false;
  }
  if (file_type != nullptr) {
    *file_type = file_header.file;
  }
  if (chunks == nullptr) {
    return true;
  }
  bool has_error = false;
  while (true) {
    auto chunk = ChunkReader::Read(file, &has_error);
    if (has_error) {
      return false;
    }
    if (!chunk.has_value()) {
      break;
    }
    chunks->emplace_back(std::move(chunk.value()));
  }
  return true;
}

}  // namespace gb
