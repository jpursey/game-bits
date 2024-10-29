// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/token.h"

#include <string>

#include "absl/strings/str_cat.h"

namespace gb {

std::string Token::ToString() const {
  switch (value_type_) {
    case ValueType::kNone:
      break;
    case ValueType::kFloat:
      return absl::StrCat(GetFloat());
    case ValueType::kInt:
      return absl::StrCat(GetInt());
    case ValueType::kString:
    case ValueType::kStringView:
      return std::string(GetString());
    case ValueType::kSymbol:
      return std::string(GetSymbol().GetString());
  }
  return "";
}

}  // namespace gb