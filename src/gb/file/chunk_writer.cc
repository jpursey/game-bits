// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/chunk_writer.h"

#include <cstring>
#include <limits>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "gb/base/allocator.h"
#include "gb/file/file.h"

namespace gb {

namespace {

// Returns `size` rounded up to a multiple of 8, as every part of a chunk is.
int64_t PadSize(int64_t size) { return (size + 7) & ~int64_t{7}; }

}  // namespace

ChunkWriter::ChunkWriter(const ChunkType& type, int32_t version, int32_t count,
                         int32_t item_size, void* chunk_data) {
  const int64_t size = int64_t{item_size} * count;
  CHECK(count >= 0 && item_size >= 0 &&
        PadSize(size) <= std::numeric_limits<int32_t>::max())
      << "Chunk " << type.ToString() << " is too large: " << count
      << " items of " << item_size << " bytes";
  header_.type = type;
  header_.version = version;
  header_.count = count;
  header_.size = static_cast<int32_t>(PadSize(size));
  if (chunk_data == nullptr) {
    owns_chunk_buffer_ = true;
    chunk_buffer_size_ = header_.size;
    if (chunk_buffer_size_ > 0) {
      chunk_buffer_ = GetDefaultAllocator()->Alloc(chunk_buffer_size_);
      std::memset(chunk_buffer_, 0, chunk_buffer_size_);
    }
  } else {
    owns_chunk_buffer_ = false;
    chunk_buffer_size_ = static_cast<int32_t>(size);
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
    const int64_t padding_size =
        PadSize(chunk_buffer_size_) - chunk_buffer_size_;
    if (padding_size > 0) {
      const uint64_t padding = 0;
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
  total_size = PadSize(total_size);
  CHECK(header_.size + total_size <= std::numeric_limits<int32_t>::max())
      << "Chunk " << header_.type.ToString() << " is too large";
  const int64_t index = extra_buffer_.size();

  // The chunk data is padded when written, so extra data starts after it.
  const int64_t offset = PadSize(chunk_buffer_size_) + index * 8;
  extra_buffer_.resize(extra_buffer_.size() + (total_size / 8), 0);
  header_.size += static_cast<int32_t>(total_size);
  return {offset, static_cast<void*>(extra_buffer_.data() + index)};
}

}  // namespace gb
