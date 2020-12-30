// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/chunk_writer.h"

#include "gb/base/allocator.h"
#include "gb/file/file.h"
#include "glog/logging.h"

namespace gb {

ChunkWriter::ChunkWriter(const ChunkType& type, int32_t version, int32_t count,
                         int32_t item_size, void* chunk_data) {
  header_.type = type;
  header_.version = version;
  header_.count = count;
  header_.size = item_size * count;
  const int32_t remainder = header_.size % 8;
  if (remainder != 0) {
    header_.size += 8 - remainder;
  }
  if (chunk_data == nullptr) {
    owns_chunk_buffer_ = true;
    chunk_buffer_size_ = header_.size;
    chunk_buffer_ = GetDefaultAllocator()->Alloc(chunk_buffer_size_);
    std::memset(chunk_buffer_, 0, chunk_buffer_size_);
  } else {
    owns_chunk_buffer_ = false;
    chunk_buffer_size_ = item_size * count;
    chunk_buffer_ = chunk_data;
  }
}

ChunkWriter::ChunkWriter(ChunkWriter&& other)
    : header_(other.header_),
      chunk_buffer_(std::exchange(other.chunk_buffer_, nullptr)),
      chunk_buffer_size_(std::exchange(other.chunk_buffer_size_, 0)),
      owns_chunk_buffer_(std::exchange(other.owns_chunk_buffer_, false)),
      extra_buffer_(std::move(other.extra_buffer_)) {}

ChunkWriter& ChunkWriter::operator=(ChunkWriter&& other) {
  if (&other != this) {
    if (owns_chunk_buffer_) {
      GetDefaultAllocator()->Free(chunk_buffer_);
    }
    header_ = other.header_;
    chunk_buffer_ = std::exchange(other.chunk_buffer_, nullptr);
    chunk_buffer_size_ = std::exchange(other.chunk_buffer_size_, 0);
    owns_chunk_buffer_ = std::exchange(other.owns_chunk_buffer_, false);
    extra_buffer_ = std::move(other.extra_buffer_);
  }
  return *this;
}

ChunkWriter::~ChunkWriter() {
  if (owns_chunk_buffer_) {
    GetDefaultAllocator()->Free(chunk_buffer_);
  }
}

bool ChunkWriter::Write(File* file) const {
  if (file->Write(&header_) != 1) {
    return false;
  }
  if (chunk_buffer_size_ > 0) {
    if (file->Write(chunk_buffer_, chunk_buffer_size_) != chunk_buffer_size_) {
      return false;
    }
    const int32_t remainder = chunk_buffer_size_ % 8;
    if (remainder != 0) {
      const uint64_t padding = 0;
      const int32_t padding_size = 8 - remainder;
      if (file->Write(static_cast<const void*>(&padding), padding_size) !=
          padding_size) {
        return false;
      }
    }
  }
  if (!extra_buffer_.empty() &&
      file->Write(extra_buffer_) != extra_buffer_.size()) {
    return false;
  }
  return true;
}

bool WriteChunkFile(File* file, const ChunkType& file_type,
                    absl::Span<const ChunkWriter> chunks) {
  ChunkHeader file_header = {kChunkTypeFile, 0, 1};
  file_header.file = file_type;
  if (file->Write(&file_header) != 1) {
    LOG(ERROR) << "Failed to write chunk file header";
    return false;
  }
  for (const ChunkWriter& chunk : chunks) {
    if (!chunk.Write(file)) {
      LOG(ERROR) << "Failed to write chunk " << chunk.GetType().ToString();
      return false;
    }
  }
  return true;
}

std::tuple<int64_t, void*> ChunkWriter::ReserveExtra(int64_t total_size) {
  int64_t remainder = (total_size % 8);
  if (remainder != 0) {
    total_size += 8 - remainder;
  }
  int64_t index = extra_buffer_.size();
  int64_t offset = chunk_buffer_size_ + (extra_buffer_.size() * 8);
  extra_buffer_.resize(extra_buffer_.size() + (total_size / 8), 0);
  header_.size += static_cast<int32_t>(total_size);
  return {offset, static_cast<void*>(extra_buffer_.data() + index)};
}

}  // namespace gb
