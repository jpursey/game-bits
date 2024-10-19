// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_SYMBOL_H_
#define GB_PARSE_SYMBOL_H_

#include <cstdint>
#include <string_view>

#include "absl/strings/ascii.h"

namespace gb {

// Storage for underlying symbol value.
using SymbolValue = uint64_t;

// Maximum size of a symbol in characters.
inline constexpr int kMaxSymbolSize = sizeof(SymbolValue) - 1;

// Symbols are used to represent token values that are intended to separate
// other types of tokens.
//
// Symbol matching rules:
// - Longer symbols match first over shorter symbols. This allows for symbols
//   that are prefixes of other symbols to be matched correctly. For instance,
//   the "<<" symbol is matched before the "<" symbol.
// - Symbols are matched first if the previous token was *not* a symbol, and
//   matched last otherwise. This allows for unambiguous parsing of symbols that
//   are prefixes of other tokens (for instance the "-" symbol and the beginning
//   of a negative number).
//
// Examples:
//   // Comparisons against characters and constant strings are both valid (and
//   // are reduced down to a simple integer comparison in optimized builds).
//   if (symbol == '+') {
//     // Handle addition.
//   } else if (symbol == "<<") {
//     // Handle left shift.
//   }
class Symbol {
 public:
  constexpr Symbol() = default;
  constexpr Symbol(const Symbol&) noexcept = default;
  constexpr Symbol& operator=(const Symbol&) noexcept = default;
  constexpr ~Symbol() = default;

  // Implicit constructors from a character or string literal.
  Symbol(SymbolValue value) noexcept;
  constexpr Symbol(char ch) noexcept : value_{ch} {}
  constexpr Symbol(std::string_view str) noexcept;
  constexpr Symbol(const char* str) noexcept : Symbol(std::string_view(str)) {}
  Symbol(const std::string& str) noexcept : Symbol(std::string_view(str)) {}

  // Returns true if the symbol is valid (contains only ASCII characters between
  // 33 and 127).
  constexpr bool IsValid() const;

  // Returns the underlying symbol value.
  SymbolValue GetValue() const;

  // Returns the size of the symbol in characters.
  constexpr int GetSize() const;

  // Returns the symbol value as a string.
  constexpr std::string_view GetString() const;

  // Full comparison operators are supported.
  //
  // Note: This is not defaulted because it is much faster in optimized builds.
  auto operator<=>(const Symbol& other) const {
    return GetValue() <=> other.GetValue();
  }
  bool operator==(const Symbol& other) const {
    return GetValue() == other.GetValue();
  }

 private:
  char value_[kMaxSymbolSize + 1] = {};
};
static_assert(sizeof(Symbol) == kMaxSymbolSize + 1);

template <typename Sink>
inline void AbslStringify(Sink& sink, const Symbol& symbol) {
  absl::Format(&sink, "\"%s\"", symbol.GetString());
}

template <typename H>
H AbslHashValue(H h, const Symbol& symbol) {
  return H::combine(std::move(h), symbol.GetValue());
}

inline Symbol::Symbol(SymbolValue value) noexcept {
  // This cannot be made constexpr because of the memcpy, but this is much
  // faster in optimized builds.
  std::memcpy(value_, &value, sizeof(value));
  value_[kMaxSymbolSize] = 0;
}

constexpr Symbol::Symbol(std::string_view str) noexcept {
  for (int i = 0; i < std::min<int>(str.size(), kMaxSymbolSize); ++i) {
    value_[i] = str[i];
  }
}

inline SymbolValue Symbol::GetValue() const {
  // This cannot be made constexpr because of the memcpy, but this is much
  // faster in optimized builds.
  SymbolValue value;
  std::memcpy(&value, value_, sizeof(value));
  return value;
}

constexpr int Symbol::GetSize() const {
  return std::string_view(value_).size();
}

constexpr std::string_view Symbol::GetString() const {
  return std::string_view(value_);
}

constexpr bool Symbol::IsValid() const {
  if (value_[0] == 0) {
    return false;
  }
  for (char ch : value_) {
    if (ch == 0) {
      return true;
    }
    if (!absl::ascii_isgraph(ch)) {
      return false;
    }
  }
  return false;
}

}  // namespace gb

#endif  // GB_PARSE_SYMBOL_H_
