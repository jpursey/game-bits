// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/raw_file.h"

namespace gb {

bool CopyRawFile(RawFile* from, RawFile* to) {
  uint8_t buffer[kRawFileCopyBufferSize];
  int64_t read_size = 0;
  do {
    read_size = from->Read(buffer, kRawFileCopyBufferSize);
    if (read_size < 0) {
      return false;
    }
    if (read_size > 0 && to->Write(buffer, read_size) != read_size) {
      return false;
    }
  } while (read_size == kRawFileCopyBufferSize);
  return to->Flush();
}

}  // namespace gb
