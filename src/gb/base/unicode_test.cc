// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/unicode.h"

#include <locale>
#include <string>
#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {

using ::testing::IsEmpty;

inline void PrintTo(const StringEncoding& value, std::ostream* out) {
  switch (value) {
    case StringEncoding::kUnknown:
      *out << "kUnknown";
      break;
    case StringEncoding::kAscii:
      *out << "kAscii";
      break;
    case StringEncoding::kUtf8:
      *out << "kUtf8";
      break;
    case StringEncoding::kUtf8WithBom:
      *out << "kUtf8WithBom";
      break;
    case StringEncoding::kUtf16:
      *out << "kUtf16";
      break;
    case StringEncoding::kUtf16WithBom:
      *out << "kUtf16WithBom";
      break;
    default:
      *out << static_cast<std::underlying_type_t<StringEncoding>>(value);
  }
}

namespace {

std::string_view ToBytes(std::u16string_view u16_string) {
  return std::string_view(reinterpret_cast<const char*>(u16_string.data()),
                          u16_string.size() * 2);
}

std::u32string CreateAllValidCodePoints() {
  std::u32string all;
  all.reserve(0x110000);
  for (char32_t ch = 1; ch < 0xD800; ++ch) {
    all.push_back(ch);
  }
  for (char32_t ch = 0xE000; ch < 0xFDD0; ++ch) {
    all.push_back(ch);
  }
  for (char32_t ch = 0xFDF0; ch < 0x110000; ++ch) {
    if ((ch & 0xFFFE) != 0xFFFE) {
      all.push_back(ch);
    }
  }
  return all;
}

std::u32string CreateAllInvalidCodePoints() {
  std::u32string all;
  all.reserve(2200);
  for (char32_t ch = 0xD800; ch < 0xE000; ++ch) {
    all.push_back(ch);
  }
  for (char32_t ch = 0xFDD0; ch < 0xFDF0; ++ch) {
    all.push_back(ch);
  }
  for (char32_t ch = 0; ch < 0x110000; ch += 0x10000) {
    all.push_back(ch | 0xFFFE);
    all.push_back(ch | 0xFFFF);
  }
  return all;
}

TEST(UnicodeTest, ProcessAllValidCodePoints) {
  const std::u32string u32_all = CreateAllValidCodePoints();

  const std::string u8_all =
      std::wstring_convert<std::codecvt<char32_t, char, std::mbstate_t>,
                           char32_t>(std::string(), std::u32string())
          .to_bytes(u32_all.data(), u32_all.data() + u32_all.size());
  ASSERT_FALSE(u8_all.empty());
  EXPECT_EQ(GetStringEncoding(u8_all, nullptr), StringEncoding::kUtf8);

  const std::u16string u16_all =
      std::wstring_convert<std::codecvt<char16_t, char, std::mbstate_t>,
                           char16_t>(std::string(), std::u16string())
          .from_bytes(u8_all.data(), u8_all.data() + u8_all.size());
  ASSERT_FALSE(u16_all.empty());
  EXPECT_EQ(GetStringEncoding(ToBytes(u16_all), nullptr),
            StringEncoding::kUtf16);

  std::string u8_result = ToUtf8(u16_all);
  EXPECT_EQ(u8_result, u8_all);
  u8_result.clear();

  std::u16string u16_result = ToUtf16(u8_all);
  EXPECT_EQ(u16_result, u16_all);
  u16_result.clear();

  const unsigned char u8_bom[3] = {0xEF, 0xBB, 0xBF};
  std::string u8_bom_all = u8_all;
  u8_bom_all.insert(0, reinterpret_cast<const char*>(u8_bom), 3);
  EXPECT_EQ(GetStringEncoding(u8_bom_all, nullptr),
            StringEncoding::kUtf8WithBom);

  u16_result = ToUtf16(u8_bom_all);
  EXPECT_EQ(u16_result, u16_all);
  u16_result.clear();

  std::u16string u16_bom_all = u16_all;
  u16_bom_all.insert(u16_bom_all.begin(), 0xFEFF);
  EXPECT_EQ(GetStringEncoding(ToBytes(u16_bom_all), nullptr),
            StringEncoding::kUtf16WithBom);

  u8_result = ToUtf8(u16_all);
  EXPECT_EQ(u8_result, u8_all);
  u8_result.clear();

  for (char16_t& ch : u16_bom_all) {
    ch = ((ch >> 8) | (ch << 8));
  }
  EXPECT_EQ(GetStringEncoding(ToBytes(u16_bom_all), nullptr),
            StringEncoding::kUtf16WithBom)
      << "Byte swapped";

  u8_result = ToUtf8(u16_bom_all);
  EXPECT_EQ(u8_result, u8_all);
  u8_result.clear();
}

TEST(UnicodeTest, GetEncodingForInvalidCodePointsForUtf8) {
  std::u32string u32_all = CreateAllInvalidCodePoints();

  // UTF-8 also can technically represent code point values higher than
  // 0x10FFFF, so add some of those in addition (no need to be exhaustive).
  for (char32_t ch = 0x110000; ch < 0x200000; ch += 0x1234) {
    u32_all.push_back(ch);
  }

  // Add byte order mark in to ensure only the validator we want to run is run.
  unsigned char u8_encode[8] = {0xEF, 0xBB, 0xBF};

  for (char32_t ch : u32_all) {
    // Manually encode, as it is invalid.
    if (ch < 0x10000) {
      u8_encode[3] = 0xE0 | static_cast<unsigned char>(ch >> 12);
      u8_encode[4] = 0x80 | static_cast<unsigned char>((ch >> 6) & 0x3F);
      u8_encode[5] = 0x80 | static_cast<unsigned char>(ch & 0x3F);
      u8_encode[6] = 0;
    } else {
      u8_encode[3] = 0xF0 | static_cast<unsigned char>(ch >> 18);
      u8_encode[4] = 0x80 | static_cast<unsigned char>((ch >> 12) & 0x3F);
      u8_encode[5] = 0x80 | static_cast<unsigned char>((ch >> 6) & 0x3F);
      u8_encode[6] = 0x80 | static_cast<unsigned char>(ch & 0x3F);
      u8_encode[7] = 0;
    }
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Character: " << static_cast<int>(ch);
  }
}

TEST(UnicodeTest, GetEncodingOverlongForUtf8) {
  // Add byte order mark in to ensure only the validator we want to run is run.
  unsigned char u8_encode[8] = {0xEF, 0xBB, 0xBF};

  for (char32_t ch = 0; ch < 0x80; ++ch) {
    u8_encode[3] = 0xC0 | static_cast<unsigned char>(ch >> 6);
    u8_encode[4] = 0x80 | static_cast<unsigned char>(ch & 0x3F);
    u8_encode[5] = 0;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Bytes: 2, Character: " << static_cast<int>(ch);

    u8_encode[3] = 0xE0;
    u8_encode[4] = 0x80 | static_cast<unsigned char>(ch >> 6);
    u8_encode[5] = 0x80 | static_cast<unsigned char>(ch & 0x3F);
    u8_encode[6] = 0;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Bytes: 3, Character: " << static_cast<int>(ch);

    u8_encode[3] = 0xF0;
    u8_encode[4] = 0x80;
    u8_encode[5] = 0x80 | static_cast<unsigned char>(ch >> 6);
    u8_encode[6] = 0x80 | static_cast<unsigned char>(ch & 0x3F);
    u8_encode[7] = 0;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Bytes: 4, Character: " << static_cast<int>(ch);
  }

  for (char32_t ch = 0x80; ch < 0x800; ++ch) {
    u8_encode[3] = 0xE0 | static_cast<unsigned char>(ch >> 12);
    u8_encode[4] = 0x80 | static_cast<unsigned char>((ch >> 6) & 0x3F);
    u8_encode[5] = 0x80 | static_cast<unsigned char>(ch & 0x3F);
    u8_encode[6] = 0;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Bytes: 3, Character: " << static_cast<int>(ch);

    u8_encode[3] = 0xF0;
    u8_encode[4] = 0x80 | static_cast<unsigned char>(ch >> 12);
    u8_encode[5] = 0x80 | static_cast<unsigned char>((ch >> 6) & 0x3F);
    u8_encode[6] = 0x80 | static_cast<unsigned char>(ch & 0x3F);
    u8_encode[7] = 0;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Bytes: 4, Character: " << static_cast<int>(ch);
  }

  for (char32_t ch = 0x800; ch < 0x1000; ++ch) {
    u8_encode[3] = 0xF0 | static_cast<unsigned char>(ch >> 18);
    u8_encode[4] = 0x80 | static_cast<unsigned char>((ch >> 12) & 0x3F);
    u8_encode[5] = 0x80 | static_cast<unsigned char>((ch >> 6) & 0x3F);
    u8_encode[6] = 0x80 | static_cast<unsigned char>(ch & 0x3F);
    u8_encode[7] = 0;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Bytes: 4, Character: " << static_cast<int>(ch);
  }
}

TEST(UnicodeTest, GetEncodingForInvalidLeadingByteForUtf8) {
  // Add byte order mark in to ensure only the validator we want to run is run.
  unsigned char u8_encode[8] = {0xEF, 0xBB, 0xBF};

  for (unsigned char ch = 0x80; ch < 0xC0; ++ch) {
    u8_encode[3] = ch;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Byte: " << (int)ch;
  }

  for (unsigned char ch = 0xF8; ch < 0xFF; ++ch) {
    u8_encode[3] = ch;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Byte: " << (int)ch;
  }
}

TEST(UnicodeTest, GetEncodingForInvalidNextByteForUtf8) {
  // Add byte order mark in to ensure only the validator we want to run is run.
  unsigned char u8_encode[8] = {0xEF, 0xBB, 0xBF};

  u8_encode[3] = 0xC2;
  for (unsigned char ch = 0xC0; ch < 0xFF; ++ch) {
    u8_encode[4] = ch;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Length: 2, Byte: " << (int)ch;
  }

  u8_encode[3] = 0xE1;
  u8_encode[4] = 0x80;
  for (unsigned char ch = 0xC0; ch < 0xFF; ++ch) {
    u8_encode[5] = ch;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Length 3, Byte: " << (int)ch;
  }

  u8_encode[3] = 0xF1;
  u8_encode[4] = 0x80;
  u8_encode[5] = 0x80;
  for (unsigned char ch = 0xC0; ch < 0xFF; ++ch) {
    u8_encode[6] = ch;
    ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
              StringEncoding::kUnknown)
        << "Length 4, Byte: " << (int)ch;
  }
}

TEST(UnicodeTest, GetEncodingForTruncatedSequenceForUtf8) {
  // Add byte order mark in to ensure only the validator we want to run is run.
  unsigned char u8_encode[8] = {0xEF, 0xBB, 0xBF};

  u8_encode[3] = 0xC2;
  ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
            StringEncoding::kUnknown)
      << "Length: 2";

  u8_encode[3] = 0xE1;
  u8_encode[4] = 0x80;
  ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
            StringEncoding::kUnknown)
      << "Length: 3";

  u8_encode[3] = 0xF1;
  u8_encode[4] = 0x80;
  u8_encode[5] = 0x80;
  ASSERT_EQ(GetStringEncoding(reinterpret_cast<char*>(u8_encode), nullptr),
            StringEncoding::kUnknown)
      << "Length: 4";
}

TEST(UnicodeTest, GetEncodingForInvalidCodePointsForUtf16) {
  std::u32string u32_all = CreateAllInvalidCodePoints();

  // Add byte order mark in to ensure only the validator we want to run is run.
  char16_t u16_encode[4] = {0xFEFF};

  for (char32_t ch : u32_all) {
    // Manually encode, as it is invalid.
    if (ch < 0x10000) {
      u16_encode[1] = static_cast<char16_t>(ch);
      u16_encode[2] = 0;
    } else {
      ch -= 0x10000;
      u16_encode[1] = 0xD800 | static_cast<char16_t>(ch >> 10);
      u16_encode[2] = 0xDC00 | static_cast<char16_t>(ch & 0x3FF);
      u16_encode[3] = 0;
    }
    ASSERT_EQ(GetStringEncoding(ToBytes(u16_encode), nullptr),
              StringEncoding::kUnknown)
        << "Character: " << static_cast<int>(ch);
  }
}

TEST(UnicodeTest, GetEncodingForInvalidSurrogatePairForUtf16) {
  // Add byte order mark in to ensure only the validator we want to run is run.
  char16_t u16_encode[4] = {0xFEFF};

  for (char16_t ch = 0; ch < 0x400; ++ch) {
    u16_encode[1] = 0xDC00 | ch;
    u16_encode[2] = 0xDC00;
    ASSERT_EQ(GetStringEncoding(ToBytes(u16_encode), nullptr),
              StringEncoding::kUnknown)
        << "0xDC00 + byte, 0xDC00: " << (int)ch;

    u16_encode[1] = 0xDC00 | ch;
    u16_encode[2] = 0xD800;
    ASSERT_EQ(GetStringEncoding(ToBytes(u16_encode), nullptr),
              StringEncoding::kUnknown)
        << "0xDC00 + byte, 0xD800: " << (int)ch;

    u16_encode[1] = 0xDC00 | ch;
    u16_encode[2] = 0;
    ASSERT_EQ(GetStringEncoding(ToBytes(u16_encode), nullptr),
              StringEncoding::kUnknown)
        << "0xDC00 + byte, null: " << (int)ch;

    u16_encode[1] = 0xD800 | ch;
    u16_encode[2] = 0;
    ASSERT_EQ(GetStringEncoding(ToBytes(u16_encode), nullptr),
              StringEncoding::kUnknown)
        << "0xD800 + byte, null: " << (int)ch;
  }
}

TEST(UnicodeTest, GetEncodingForOddByteCountForUtf16) {
  // Add byte order mark in to ensure only the validator we want to run is run.
  const char16_t u16_encode[4] = {0xFEFF, 0x2030, 0x4050, 0};

  const char* bytes = reinterpret_cast<const char*>(u16_encode);
  ASSERT_EQ(GetStringEncoding(std::string_view(bytes, 3), nullptr),
            StringEncoding::kUnknown)
      << "3 bytes";
  ASSERT_EQ(GetStringEncoding(std::string_view(bytes, 3), nullptr),
            StringEncoding::kUnknown)
      << "5 bytes";
}

TEST(UnicodeTest, GetEncodingReturnsUtf16String) {
  std::u16string test_string = u"This is a test string!";
  std::u16string result_string;
  ASSERT_EQ(GetStringEncoding(ToBytes(test_string), &result_string),
            StringEncoding::kUtf16);
  EXPECT_EQ(result_string, test_string);
  result_string.clear();

  std::u16string bom_test_string = test_string;
  bom_test_string.insert(bom_test_string.begin(), 0xFEFF);
  ASSERT_EQ(GetStringEncoding(ToBytes(bom_test_string), &result_string),
            StringEncoding::kUtf16WithBom);
  EXPECT_EQ(result_string, test_string);
  result_string.clear();

  std::u16string swapped_test_string = bom_test_string;
  for (char16_t& ch : swapped_test_string) {
    ch = ((ch >> 8) | (ch << 8));
  }
  ASSERT_EQ(GetStringEncoding(ToBytes(swapped_test_string), &result_string),
            StringEncoding::kUtf16WithBom);
  EXPECT_EQ(result_string, test_string);
  result_string.clear();
}

TEST(UnicodeTest, GetEncodingForNullCodePoint) {
  // Add byte order mark in to ensure only the validator we want to run is run.
  const unsigned char u8_encode[4] = {0xEF, 0xBB, 0xBF, 0};
  const char16_t u16_encode[2] = {0xFEFF, 0};
  ASSERT_EQ(GetStringEncoding(
                std::string_view(reinterpret_cast<const char*>(u8_encode), 4),
                nullptr),
            StringEncoding::kUnknown);
  ASSERT_EQ(GetStringEncoding(
                std::string_view(reinterpret_cast<const char*>(u16_encode), 4),
                nullptr),
            StringEncoding::kUnknown);
}

TEST(UnicodeTest, EmptyString) {
  const unsigned char u8_bom_bytes[4] = {0xEF, 0xBB, 0xBF, 0};
  const char* const u8_bom = reinterpret_cast<const char*>(u8_bom_bytes);
  const char16_t u16_bom[2] = {0xFEFF, 0};
  const char16_t u16_swap_bom[2] = {0xFFFE, 0};

  EXPECT_THAT(ToUtf8({}), IsEmpty());
  EXPECT_THAT(ToUtf8(u16_bom), IsEmpty());
  EXPECT_THAT(ToUtf8(u16_swap_bom), IsEmpty());

  EXPECT_THAT(ToUtf16({}), IsEmpty());
  EXPECT_THAT(ToUtf16(u8_bom), IsEmpty());

  EXPECT_EQ(GetStringEncoding("", nullptr), StringEncoding::kAscii);
  EXPECT_EQ(GetStringEncoding(u8_bom, nullptr), StringEncoding::kUtf8WithBom);
  EXPECT_EQ(GetStringEncoding(ToBytes(u16_bom), nullptr),
            StringEncoding::kUtf16WithBom);
  EXPECT_EQ(GetStringEncoding(ToBytes(u16_swap_bom), nullptr),
            StringEncoding::kUtf16WithBom);
}

}  // namespace
}  // namespace gb
