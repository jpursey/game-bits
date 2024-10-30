// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parse_error.h"

#include "absl/strings/str_cat.h"

namespace gb {

std::string ParseError::FormatMessage() const {
  if (location_.line < 0) {
    return message_;
  }
  return absl::StrCat(location_.filename, "(", location_.line, "): ", message_);
}

}  // namespace gb

