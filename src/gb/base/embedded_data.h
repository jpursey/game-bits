// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_EMBEDDED_DATA_H_
#define GB_BASE_EMBEDDED_DATA_H_

#include <cstdint>
#include <string_view>

#include "absl/types/span.h"

namespace gb {

// The contents of a file built into the program by gb_embed_data (see
// CMake/GameBitsTargetCommands.cmake), whose generated header declares one of
// these.
//
// The data is exactly the file's bytes, with nothing escaped or converted. It
// starts on a 4-byte boundary, so data made of 32-bit words (such as SPIR-V)
// can be read as uint32_t. It is followed by a zero byte that isn't part of it,
// so GetText().data() is a null-terminated string.
class EmbeddedData final {
 public:
  // Called by the generated code: `data` is `size` bytes followed by a zero,
  // and lives as long as the program.
  constexpr EmbeddedData(const uint8_t* data, int size)
      : data_(data), size_(size) {}

  std::string_view GetText() const {
    return std::string_view(reinterpret_cast<const char*>(data_), size_);
  }
  absl::Span<const uint8_t> GetBytes() const {
    return absl::MakeConstSpan(data_, size_);
  }

 private:
  const uint8_t* data_;
  int size_;
};

}  // namespace gb

#endif  // GB_BASE_EMBEDDED_DATA_H_
