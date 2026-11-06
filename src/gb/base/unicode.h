// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_UNICODE_H_
#define GB_BASE_UNICODE_H_

#include <optional>
#include <string>
#include <string_view>

namespace gb {

// Defines the string encodings with optional byte order mark (BOM) supported by
// this module.
enum class StringEncoding {
  kUnknown,       // An unsupported string encoding, or raw binary data.
  kAscii,         // All characters are 7-bit ASCII. Never contains BOM.
  kUtf8,          // UTF-8 encoded string.
  kUtf8WithBom,   // UTF-8 encoded string with BOM prefix.
  kUtf16,         // UTF-16 encoded string.
  kUtf16WithBom,  // UTF-16 encoded string with BOM prefix.
};

// Determines and validates the string encoding of a byte string.
//
// This only validates that code points are valid for the detected encoding. It
// does not validate that the resulting string (if displayed) would be valid
// (for instance format code points in a non-sensical location, or the presence
// of private-use code points).
//
// If utf16_string is specified, and the string is a valid UTF-16 encoding, it
// will be filled with the UTF-16 string data (doing any endian coversion if
// necessary). This string may be passed to ToUtf8 safely for any further
// conversion. The resulting string never has any BOM prefix.
//
// If multiple encodings are possible for the string, it will return the
// encoding in this order of preference: kAscii -> kUtf8 -> kUtf16. Strings with
// a byte order mark are never ambiguous.
//
// If a valid string encoding could not be determined, this returns kUnknown.
StringEncoding GetStringEncoding(std::string_view string,
                                 std::u16string* utf16_string = nullptr);

// Converts an ASCII or UTF-8-encoded string to UTF-16.
//
// This will not work with non-UTF-8 conforming character data (including
// extended ASCII or other non-US-ASCII encoding). If any decoding or encoding
// error occurs, this will return an empty string. If the nature of the string
// encoding is unknown, call GetStringEncoding first to ensure this is safe to
// call (kAscii, kUtf8, and kUtf8WithBom are valid).
//
// Note: On Windows, a string may safely converted to UTF-16 WCHAR this way:
//   std::u16string utf16_str = ToUtf16(...);
//   const WCHAR* wchar_str = reinterpret_cast<WCHAR*>(utf16_string.c_str());
//
// This does handle the UTF-8 byte order mark (BOM) if it exists. It never
// outputs the UTF-16 BOM, however.
std::u16string ToUtf16(std::string_view utf8_string);

// Converts a UTF-16-encoded string to UTF-8.
//
// This will not work with non-UTF-16 conforming character data. If any decoding
// or encoding error occurs, this will return an empty string. If the nature of
// the string encoding is unknown, call GetStringEncoding first to ensure this
// is safe to call (kUtf16 and kUtf16WithBom are valid).
//
// Note: On Windows, a WCHAR string may converted to UTF-8 this way:
//   const WCHAR* wchar_str = ...;  // Null-terminated.
//   std::string utf8_str = ToUtf8(
//       std::u16string_view(reinterpret_cast<const char16_t*>(wchar_c_str)));
//
// This does handle the UTF-16 byte order mark (BOM) if it exists. It never
// outputs the UTF-8 BOM, however.
std::string ToUtf8(std::u16string_view utf16_string);

//==============================================================================
// Code points
//
// These work on Unicode scalar values: the code points from U+0000 to U+10FFFF,
// other than the UTF-16 surrogates (U+D800 to U+DFFF). Noncharacters (such as
// U+FFFE) and U+0000 are valid. A byte order mark (U+FEFF) is a code point like
// any other, and is never added or removed.
//==============================================================================

// Converts a UTF-8-encoded string to UTF-32.
//
// If the string is not valid UTF-8, this returns an empty string. Unlike
// ToUtf16(), a leading byte order mark is kept as a code point.
std::u32string ToUtf32(std::string_view utf8_string);

// Converts a UTF-32-encoded string to UTF-8.
//
// If any code point in the string is invalid, this returns an empty string.
// Unlike ToUtf8(std::u16string_view), a leading byte order mark is kept as a
// code point.
std::string ToUtf8(std::u32string_view utf32_string);

// Appends the UTF-8 encoding of `code_point` to `utf8_string`.
//
// If `code_point` is invalid, this returns false and leaves `utf8_string`
// unchanged.
bool AppendUtf8(std::string* utf8_string, char32_t code_point);

// Removes the first code point from the front of a UTF-8 or UTF-16 string, and
// returns it.
//
// If the string is empty, or does not start with a valid encoding of a code
// point (including a UTF-8 sequence or UTF-16 surrogate pair that is cut short
// by the end of the string), this returns std::nullopt and leaves the string
// unchanged. A loop that consumes code points until this fails can tell the end
// of the string from invalid input by whether the string is empty.
std::optional<char32_t> ConsumeCodePoint(std::string_view* utf8_string);
std::optional<char32_t> ConsumeCodePoint(std::u16string_view* utf16_string);

}  // namespace gb

#endif  // GB_BASE_UNICODE_H_
