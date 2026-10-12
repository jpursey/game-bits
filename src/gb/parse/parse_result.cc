// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parse_result.h"

namespace gb {

absl::Span<const ParsedItem> ParsedItem::GetItems(std::string_view name) const {
  const ParsedItem* item = this;
  for (auto dot = name.find('.'); dot != std::string_view::npos;
       dot = name.find('.')) {
    item = item->GetItem(name.substr(0, dot));
    if (item == nullptr) {
      return {};
    }
    name.remove_prefix(dot + 1);
  }
  auto it = item->items_.find(name);
  if (it == item->items_.end()) {
    return {};
  }
  return it->second;
}

}  // namespace gb
