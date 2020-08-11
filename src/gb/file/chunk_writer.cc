// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/chunk_writer.h"

#include "gb/file/file.h"
#include "glog/logging.h"

namespace gb {

bool ChunkWriter::Write(File* file) const {
  return file->Write(&header_) == 1 &&
         file->Write(chunk_buffer_) == chunk_buffer_.size() &&
         file->Write(extra_buffer_) == extra_buffer_.size();
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

}  // namespace gb
