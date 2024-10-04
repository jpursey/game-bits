// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_SYMBOL_H_
#define GB_PARSE_SYMBOL_H_

#include <cstdint>
#include <string_view>

namespace gb {

// Storage for underlying symbol value.
using SymbolValue = uint64_t;

// Maximum size of a symbol in characters.
inline constexpr int kMaxSymbolSize = sizeof(SymbolValue);

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
//   constexpr Symbol kShiftLeft = "<<";
//   if (symbol == '+') {  // No need to create a symbol for simple characters.
//     // Handle addition.
//   } else if (symbol == kShiftLeft) {
//     // Handle left shift.
//   }
class Symbol {
 public:
  constexpr Symbol() = default;
  constexpr Symbol(const Symbol&) noexcept = default;
  constexpr Symbol& operator=(const Symbol&) noexcept = default;
  constexpr ~Symbol() = default;

  // Implicit constructors from a character or string literal.
  constexpr Symbol(SymbolValue value) noexcept : value_(value) {}
  constexpr Symbol(char ch) noexcept : value_(ch) {}
  constexpr Symbol(std::string_view str) noexcept;
  constexpr Symbol(const char* str) noexcept : Symbol(std::string_view(str)) {}

  // Returns the underlying symbol value.
  constexpr SymbolValue GetValue() const { return value_; }

  // Returns the size of the symbol in characters.
  constexpr int GetSize() const;

  // Returns the symbol value as a string.
  constexpr std::string_view GetString() const;

  // Full comparison operators are supported.
  constexpr auto operator<=>(const Symbol&) const = default;

 private:
  static constexpr SymbolValue ToValue(const char* str, size_t size) {
    SymbolValue value = 0;
    while (size > 0) {
      value = (value << 8) | *str++;
      --size;
    }
    return value;
  }

  SymbolValue value_ = 0;
};
static_assert(sizeof(Symbol) == kMaxSymbolSize);

constexpr Symbol::Symbol(std::string_view str) noexcept
    : value_(ToValue(str.data(), str.size())) {}

constexpr int Symbol::GetSize() const {
  SymbolValue value = value_;
  int size = 0;
  while (value > 0) {
    value >>= 8;
    ++size;
  }
  return size;
}

constexpr std::string_view Symbol::GetString() const {
  return std::string_view(reinterpret_cast<const char*>(&value_), GetSize());
}

}  // namespace gb

#endif  // GB_PARSE_SYMBOL_H_
