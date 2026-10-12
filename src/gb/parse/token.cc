// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/token.h"

#include <cstdint>
#include <string>
#include <variant>

#include "absl/strings/str_cat.h"

namespace gb {

TokenValue Token::GetValue() const {
  switch (value_type_) {
    case ValueType::kNone:
      return TokenValue();
    case ValueType::kFloat:
      return GetFloat();
    case ValueType::kInt:
      return GetInt();
    case ValueType::kString:
    case ValueType::kStringView:
      return std::string(GetString());
    case ValueType::kSymbol:
      return GetSymbol();
  }
  return TokenValue();
}

bool Token::HasValue(const TokenValue& value) const {
  if (std::holds_alternative<NoTokenValue>(value)) {
    return true;
  }
  switch (value_type_) {
    case ValueType::kNone:
      return false;
    case ValueType::kFloat: {
      const double* expected = std::get_if<double>(&value);
      return expected != nullptr && *expected == GetFloat();
    }
    case ValueType::kInt: {
      const int64_t* expected = std::get_if<int64_t>(&value);
      return expected != nullptr && *expected == GetInt();
    }
    case ValueType::kString:
    case ValueType::kStringView: {
      const std::string* expected = std::get_if<std::string>(&value);
      return expected != nullptr && *expected == GetString();
    }
    case ValueType::kSymbol: {
      const Symbol* expected = std::get_if<Symbol>(&value);
      return expected != nullptr && *expected == GetSymbol();
    }
  }
  return false;
}

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