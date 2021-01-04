// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_UNICODE_H_
#define GB_BASE_UNICODE_H_

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

}  // namespace gb

#endif  // GB_BASE_UNICODE_H_
