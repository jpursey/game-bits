// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parse_result.h"

namespace gb {

absl::Span<const ParsedItem> ParsedItem::GetItem(std::string_view name) const {
  absl::Span<const ParsedItem> result;
  if (auto it = items_.find(name); it != items_.end()) {
    result = absl::MakeConstSpan(it->second);
  }
  return result;
}

}  // namespace gb
