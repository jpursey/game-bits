// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_RAW_FILE_H_
#define GB_FILE_RAW_FILE_H_

#include <stdint.h>

#include "gb/file/file_types.h"

namespace gb {

// Raw file to underlying file protocol file.
//
// A raw file is closed when it is destructed. A raw file may buffer what is
// written, as long as Flush() writes it, and destroying the RawFile writes any
// that remains.
//
// Derived class of RawFile must be thread-compatible.
class RawFile {
 public:
  RawFile(const RawFile&) = delete;
  RawFile& operator=(const RawFile&) = delete;
  virtual ~RawFile() = default;

  // Seeks to the end of the file. This should return the end-of-file position,
  // or -1 on error.
  virtual int64_t SeekEnd() = 0;

  // Seeks to the requested position. This should return the actual position
  // achieved (for instance, it is allowed to truncate the position and not fail
  // if it is out of range). On error it should return -1.
  virtual int64_t SeekTo(int64_t position) = 0;

  // Writes the requested number of bytes to the file, returning the total
  // number of bytes actually written. This is only called if the file was
  // opened for writing.
  virtual int64_t Write(const void* buffer, int64_t size) = 0;

  // Writes anything buffered to the file, returning true if everything written
  // so far has reached the file. This is only called if the file was opened
  // for writing.
  virtual bool Flush() = 0;

  // Reads the requested number of bytes from the file, returning the total
  // number of bytes actually read, which is less only at the end of the file.
  // If the read fails, this returns -1, and what it put in the buffer is
  // undefined. This is only called if the file was open for reading.
  virtual int64_t Read(void* buffer, int64_t size) = 0;

 protected:
  RawFile() = default;
};

// Number of bytes CopyRawFile() copies at a time.
inline constexpr int64_t kRawFileCopyBufferSize = 32 * 1024;

// Copies the rest of `from`, from its position, to `to` at its position, and
// flushes `to`. `from` must be open for reading, and `to` for writing.
//
// Returns true if all of `from` was read and reached `to`.
bool CopyRawFile(RawFile* from, RawFile* to);

}  // namespace gb

#endif  // GB_FILE_RAW_FILE_H_
