// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parse_result.h"

#include "absl/strings/str_split.h"

namespace gb {

absl::Span<const ParsedItem> ParsedItem::GetItems(std::string_view name) const {
  const ParsedItem* item = this;
  if (name.find('.') != std::string_view::npos) {
    std::vector<std::string_view> names = absl::StrSplit(name, '.');
    std::string_view new_name = names.back();
    names.pop_back();
    for (const auto& name : names) {
      item = item->GetItem(name);
      if (item == nullptr) {
        {};
      }
    }
    name = new_name;
  }
  absl::Span<const ParsedItem> result;
  if (auto it = item->items_.find(std::string(name));
      it != item->items_.end()) {
    result = absl::MakeConstSpan(it->second);
  }
  return result;
}

}  // namespace gb
