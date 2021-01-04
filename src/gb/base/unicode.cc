// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/unicode.h"

#include <locale>

namespace gb {

namespace {

class EncodingDetector {
 public:
  explicit EncodingDetector(std::string_view str)
      : data_(reinterpret_cast<const unsigned char*>(str.data())),
        count_(static_cast<int>(str.size())) {
    Process();
  }

  StringEncoding GetEncoding() const { return encoding_; }
  void GetUtf16String(std::u16string* utf16_string) const;

 private:
  void Process();
  void ProcessByteOrderMark();
  void ValidateUtf8();
  void ValidateUtf16();

  // Result encoding.
  StringEncoding encoding_ = StringEncoding::kUnknown;

  // Encoding is determined on raw bytes. As 'char' may be signed or unsigned,
  // we force the string to be unsigned for this purpose.
  const unsigned char* data_;
  int count_;

  // The encoding is everything until options are eliminated.
  bool is_ascii_ = true;
  bool is_utf8_ = true;
  bool is_utf16_ = true;

  // Byte order mark properties.
  bool has_bom_ = false;
  bool needs_byte_swap_ = false;
};

void EncodingDetector::GetUtf16String(std::u16string* utf16_string) const {
  if (encoding_ < StringEncoding::kUtf16) {
    utf16_string->clear();
    return;
  }
  utf16_string->assign(reinterpret_cast<const char16_t*>(data_), count_ / 2);
  if (needs_byte_swap_) {
    for (char16_t& ch : *utf16_string) {
      ch = ((ch >> 8) | (ch << 8));
    }
  }
}

void EncodingDetector::ProcessByteOrderMark() {
  if (count_ < 2) {
    return;
  }

  // UTF-16
  if ((data_[0] == 0xFE && data_[1] == 0xFF) ||
      (data_[0] == 0xFF && data_[1] == 0xFE)) {
    needs_byte_swap_ = (*reinterpret_cast<const uint16_t*>(data_) != 0xFEFF);
    data_ += 2;
    count_ -= 2;
    is_ascii_ = false;
    is_utf8_ = false;
    has_bom_ = true;
    return;
  }

  // UTF-8
  if (count_ >= 3 && data_[0] == 0xEF && data_[1] == 0xBB && data_[2] == 0xBF) {
    data_ += 3;
    count_ -= 3;
    is_ascii_ = false;
    is_utf16_ = false;
    has_bom_ = true;
    return;
  }
}

void EncodingDetector::ValidateUtf8() {
  int i = 0;
  while (i < count_) {
    // Embedded null (while technically valid) is not supported.
    if (data_[i] == 0) {
      is_ascii_ = false;
      is_utf8_ = false;
      return;
    }

    // Check ASCII character set.
    if ((data_[i] & 0x80) != 0x80) {
      ++i;
      continue;
    }
    is_ascii_ = false;

    // Check for always-invalid UTF-8 values.
    unsigned char leading = data_[i++];
    if (leading == 0xC0 || leading == 0xC1 || leading >= 0xF5) {
      is_utf8_ = false;
      return;
    }

    int advance = 0;
    if ((leading & 0xE0) == 0xC0) {
      advance = 1;
    } else if ((leading & 0xF0) == 0xE0) {
      advance = 2;
    } else if ((leading & 0xF8) == 0xF0) {
      advance = 3;
    } else {
      is_utf8_ = false;
      return;
    }
    if (i + advance > count_) {
      is_utf8_ = false;
      return;
    }

    // Check for sometimes-invalid UTF-8 values
    if ((leading == 0xE0 && data_[i] < 0xA0) ||   // Overlong encoding
        (leading == 0xF0 && data_[i] < 0x90) ||   // Overlong encoding
        (leading == 0xF4 && data_[i] >= 0x90) ||  // Too big: > 0x10FFFF
        (leading == 0xED && data_[i] >= 0xA0)) {  // UTF-16 surragate pairs
      is_utf8_ = false;
      return;
    }

    // Check for any values in the range [0xFDD0, 0xFDEF], which are defined to
    // be invalid code points.
    if (leading == 0xEF && data_[i] == 0xB7 &&
        (data_[i + 1] >= 0x90 && data_[i + 1] <= 0xAF)) {
      is_utf8_ = false;
      return;
    }

    // Check for any values ending in 0xFFFF or 0xFFEE, which are defined to be
    // invalid code points.
    if (advance > 1 && (data_[i + advance - 1] & 0xFE) == 0xBE &&
        data_[i + advance - 2] == 0xBF &&
        (data_[i + advance - 3] & 0x0F) == 0x0F) {
      is_utf8_ = false;
      return;
    }

    const int end = i + advance;
    for (; i < end; ++i) {
      if ((data_[i] & 0xC0) != 0x80) {
        is_utf8_ = false;
        return;
      }
    }
  }
}

void EncodingDetector::ValidateUtf16() {
  // UTF-16 is a 2-byte format!
  if ((count_ & 1) == 1) {
    is_utf16_ = false;
    return;
  }

  // Iterate over two-byte words.
  // Note: sizeof(char16_t) must equal 2 in Game Bits. This is enforced in
  // platform_requirements.cc
  const char16_t* data = reinterpret_cast<const char16_t*>(data_);
  const int count = count_ / 2;

  int i = 0;
  while (i < count) {
    char16_t word = data[i++];
    if (needs_byte_swap_) {
      word = ((word >> 8) || (word << 8));
    }

    // Embedded null (while technically valid) is not supported.
    if (word == 0) {
      is_utf16_ = false;
      return;
    }

    // Single word encoding.
    if (word < 0xD800 || word >= 0xE000) {
      // 0xFFFF, 0xFFFE, and anything in the range [0xFDD0, 0xFDEF] are invalid
      // code points.
      if ((word & 0xFFFE) == 0xFFFE || (word >= 0xFDD0 && word <= 0xFDEF)) {
        is_utf16_ = false;
        return;
      }
      continue;
    }

    // Validate the surrogate pair.
    if (i == count) {
      is_utf16_ = false;
      return;
    }
    char16_t next_word = data[i++];
    if (needs_byte_swap_) {
      next_word = ((next_word >> 8) || (next_word << 8));
    }
    if ((word & 0xFC00) != 0xD800 || (next_word & 0xFC00) != 0xDC00) {
      is_utf16_ = false;
      return;
    }
    // Check for any values ending in 0xFFFF or 0xFFEE, which are defined to be
    // invalid code points.
    if ((word & 0x003F) == 0x003F && (next_word & 0x03FE) == 0x03FE) {
      is_utf16_ = false;
      return;
    }
  }
}

void EncodingDetector::Process() {
  ProcessByteOrderMark();

  if (is_ascii_ || is_utf8_) {
    ValidateUtf8();
    if (is_ascii_) {
      encoding_ = StringEncoding::kAscii;
      is_utf16_ = false;
    } else if (is_utf8_) {
      encoding_ =
          (has_bom_ ? StringEncoding::kUtf8WithBom : StringEncoding::kUtf8);
      is_utf16_ = false;
    }
  }

  if (is_utf16_) {
    ValidateUtf16();
    if (is_utf16_) {
      encoding_ =
          (has_bom_ ? StringEncoding::kUtf16WithBom : StringEncoding::kUtf16);
    }
  }
}

}  // namespace

StringEncoding GetStringEncoding(std::string_view str,
                                 std::u16string* utf16_string) {
  EncodingDetector detector(str);
  if (utf16_string != nullptr) {
    detector.GetUtf16String(utf16_string);
  }
  return detector.GetEncoding();
}

std::u16string ToUtf16(std::string_view utf8_string) {
  const bool has_bom = (utf8_string.size() >= 3 && utf8_string[0] == 'xEF' &&
                        utf8_string[1] == 'xBB' && utf8_string[2] == 'xBF');
  const char* begin = utf8_string.data() + (has_bom ? 3 : 0);
  const char* const end = utf8_string.data() + utf8_string.size();
  return std::wstring_convert<std::codecvt<char16_t, char, std::mbstate_t>,
                              char16_t>(std::string(), std::u16string())
      .from_bytes(begin, end);
}

std::string ToUtf8(std::u16string_view utf16_string) {
  const bool has_bom = (!utf16_string.empty() && (utf16_string[0] == 0xFEFF ||
                                                  utf16_string[0] == 0xFFFE));
  std::u16string swapped;
  if (has_bom && utf16_string[0] == 0xFFFE) {
    swapped.assign(utf16_string.data(), utf16_string.size());
    for (char16_t& ch : swapped) {
      ch = ((ch >> 8) | (ch << 8));
    }
    utf16_string = swapped;
  }
  const char16_t* const begin = utf16_string.data() + (has_bom ? 1 : 0);
  const char16_t* const end = utf16_string.data() + utf16_string.size();
  return std::wstring_convert<std::codecvt<char16_t, char, std::mbstate_t>,
                              char16_t>(std::string(), std::u16string())
      .to_bytes(begin, end);
}

}  // namespace gb
