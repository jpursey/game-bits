// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/unicode.h"

#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "utf8.h"

namespace gb {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;

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

  std::string u8_all;
  utf8::utf32to8(u32_all.begin(), u32_all.end(),
                 std::back_insert_iterator<std::string>(u8_all));
  ASSERT_FALSE(u8_all.empty());
  EXPECT_EQ(GetStringEncoding(u8_all, nullptr), StringEncoding::kUtf8);

  std::u16string u16_all;
  utf8::utf8to16(u8_all.begin(), u8_all.end(),
                 std::back_insert_iterator<std::u16string>(u16_all));
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

  EXPECT_THAT(ToUtf8(std::u16string_view()), IsEmpty());
  EXPECT_THAT(ToUtf8(std::u32string_view()), IsEmpty());
  EXPECT_THAT(ToUtf8(u16_bom), IsEmpty());
  EXPECT_THAT(ToUtf8(u16_swap_bom), IsEmpty());

  EXPECT_THAT(ToUtf16({}), IsEmpty());
  EXPECT_THAT(ToUtf16(u8_bom), IsEmpty());

  EXPECT_THAT(ToUtf32({}), IsEmpty());

  EXPECT_EQ(GetStringEncoding("", nullptr), StringEncoding::kAscii);
  EXPECT_EQ(GetStringEncoding(u8_bom, nullptr), StringEncoding::kUtf8WithBom);
  EXPECT_EQ(GetStringEncoding(ToBytes(u16_bom), nullptr),
            StringEncoding::kUtf16WithBom);
  EXPECT_EQ(GetStringEncoding(ToBytes(u16_swap_bom), nullptr),
            StringEncoding::kUtf16WithBom);
}

TEST(UnicodeTest, ToUtf16InvalidUtf8IsEmpty) {
  EXPECT_THAT(ToUtf16("a\xFF"), IsEmpty());             // Invalid byte.
  EXPECT_THAT(ToUtf16("a\xC3"), IsEmpty());             // Truncated sequence.
  EXPECT_THAT(ToUtf16("a\xC3z"), IsEmpty());            // Bad continuation.
  EXPECT_THAT(ToUtf16("a\xC0\xAF"), IsEmpty());         // Overlong encoding.
  EXPECT_THAT(ToUtf16("a\xED\xA0\x80"), IsEmpty());     // UTF-16 surrogate.
  EXPECT_THAT(ToUtf16("\xEF\xBB\xBF\xFF"), IsEmpty());  // Invalid after BOM.
}

TEST(UnicodeTest, ToUtf8InvalidUtf16IsEmpty) {
  EXPECT_THAT(ToUtf8(u"a\xD800"), IsEmpty());       // Truncated pair.
  EXPECT_THAT(ToUtf8(u"a\xD800z"), IsEmpty());      // Lead without trail.
  EXPECT_THAT(ToUtf8(u"a\xDC00z"), IsEmpty());      // Trail without lead.
  EXPECT_THAT(ToUtf8(u"\xFFFE\x00D8"), IsEmpty());  // Swapped, after BOM.
}

TEST(UnicodeTest, ToUtf8SurrogatePair) {
  EXPECT_EQ(ToUtf8(u"a\xD83D\xDE00z"), "a\xF0\x9F\x98\x80z");
  EXPECT_EQ(ToUtf8(u"\xFFFE\x3DD8\x00DE"), "\xF0\x9F\x98\x80");
}

// Every Unicode scalar value, including U+0000 and the noncharacters, in each
// encoding.
struct AllScalarValues {
  AllScalarValues() {
    utf32_string.reserve(0x110000 - 0x800);
    for (char32_t ch = 0; ch < 0xD800; ++ch) {
      utf32_string.push_back(ch);
    }
    for (char32_t ch = 0xE000; ch < 0x110000; ++ch) {
      utf32_string.push_back(ch);
    }
    utf8::unchecked::utf32to8(
        utf32_string.begin(), utf32_string.end(),
        std::back_insert_iterator<std::string>(utf8_string));
    utf8::unchecked::utf8to16(
        utf8_string.begin(), utf8_string.end(),
        std::back_insert_iterator<std::u16string>(utf16_string));
  }

  std::u32string utf32_string;
  std::string utf8_string;
  std::u16string utf16_string;
};

// Returns the scalar values, which are built on first use.
const AllScalarValues& GetAllScalarValues() {
  static const AllScalarValues* const s_all = new AllScalarValues();
  return *s_all;
}

TEST(UnicodeTest, Utf32ConvertsAllScalarValues) {
  const AllScalarValues& all = GetAllScalarValues();
  EXPECT_EQ(ToUtf8(all.utf32_string), all.utf8_string);
  EXPECT_EQ(ToUtf32(all.utf8_string), all.utf32_string);
}

TEST(UnicodeTest, ToUtf32KeepsByteOrderMark) {
  EXPECT_EQ(ToUtf32("\xEF\xBB\xBF"
                    "a"),
            U"\xFEFF"
            U"a");
  EXPECT_EQ(ToUtf8(U"\xFEFF"
                   U"a"),
            "\xEF\xBB\xBF"
            "a");
}

TEST(UnicodeTest, ToUtf32InvalidUtf8IsEmpty) {
  EXPECT_THAT(ToUtf32("a\xFF"), IsEmpty());              // Invalid byte.
  EXPECT_THAT(ToUtf32("a\xC3"), IsEmpty());              // Truncated sequence.
  EXPECT_THAT(ToUtf32("a\xC3z"), IsEmpty());             // Bad continuation.
  EXPECT_THAT(ToUtf32("a\xC0\xAF"), IsEmpty());          // Overlong encoding.
  EXPECT_THAT(ToUtf32("a\xED\xA0\x80"), IsEmpty());      // UTF-16 surrogate.
  EXPECT_THAT(ToUtf32("a\xF4\x90\x80\x80"), IsEmpty());  // Above U+10FFFF.
}

TEST(UnicodeTest, ToUtf8InvalidUtf32IsEmpty) {
  EXPECT_THAT(ToUtf8(U"a\xD800"), IsEmpty());    // Lead surrogate.
  EXPECT_THAT(ToUtf8(U"a\xDFFF"), IsEmpty());    // Trail surrogate.
  EXPECT_THAT(ToUtf8(U"a\x110000"), IsEmpty());  // Above U+10FFFF.
}

TEST(UnicodeTest, AppendUtf8AllScalarValues) {
  const AllScalarValues& all = GetAllScalarValues();
  std::string text;
  for (const char32_t ch : all.utf32_string) {
    ASSERT_TRUE(AppendUtf8(&text, ch)) << "Character: " << static_cast<int>(ch);
  }
  EXPECT_EQ(text, all.utf8_string);
}

TEST(UnicodeTest, AppendUtf8EachLength) {
  std::string text = "a";
  EXPECT_TRUE(AppendUtf8(&text, U'z'));
  EXPECT_TRUE(AppendUtf8(&text, 0xE9));
  EXPECT_TRUE(AppendUtf8(&text, 0x20AC));
  EXPECT_TRUE(AppendUtf8(&text, 0x1F600));
  EXPECT_EQ(text,
            "az\xC3\xA9\xE2\x82\xAC"
            "\xF0\x9F\x98\x80");
}

TEST(UnicodeTest, AppendUtf8InvalidLeavesStringUnchanged) {
  std::string text = "a";
  EXPECT_FALSE(AppendUtf8(&text, 0xD800));
  EXPECT_FALSE(AppendUtf8(&text, 0xDFFF));
  EXPECT_FALSE(AppendUtf8(&text, 0x110000));
  EXPECT_FALSE(AppendUtf8(&text, 0xFFFFFFFF));
  EXPECT_EQ(text, "a");
}

TEST(UnicodeTest, ConsumeCodePointAllScalarValuesUtf8) {
  const AllScalarValues& all = GetAllScalarValues();
  std::string_view text = all.utf8_string;
  std::u32string consumed;
  while (const std::optional<char32_t> ch = ConsumeCodePoint(&text)) {
    consumed.push_back(*ch);
  }
  EXPECT_THAT(text, IsEmpty());
  EXPECT_EQ(consumed, all.utf32_string);
}

TEST(UnicodeTest, ConsumeCodePointAllScalarValuesUtf16) {
  const AllScalarValues& all = GetAllScalarValues();
  std::u16string_view text = all.utf16_string;
  std::u32string consumed;
  while (const std::optional<char32_t> ch = ConsumeCodePoint(&text)) {
    consumed.push_back(*ch);
  }
  EXPECT_THAT(text, IsEmpty());
  EXPECT_EQ(consumed, all.utf32_string);
}

TEST(UnicodeTest, ConsumeCodePointEmpty) {
  std::string_view u8_text;
  EXPECT_THAT(ConsumeCodePoint(&u8_text), Eq(std::nullopt));
  std::u16string_view u16_text;
  EXPECT_THAT(ConsumeCodePoint(&u16_text), Eq(std::nullopt));
}

TEST(UnicodeTest, ConsumeCodePointInvalidUtf8LeavesStringUnchanged) {
  const std::string_view kInvalid[] = {
      "\xFF",              // Invalid byte.
      "\xF5\x80\x80\x80",  // Lead byte above U+10FFFF.
      "\xF8\x80\x80\x80",  // Lead byte of a 5-byte sequence.
      "\x80z",             // Continuation byte with no lead.
      "\xC3",              // Truncated sequence.
      "\xE2\x82",          // Truncated sequence.
      "\xF0\x9F\x98",      // Truncated sequence.
      "\xC3z",             // Bad continuation.
      "\xE2z\xAC",         // Bad continuation.
      "\xC0\xAF",          // Overlong encoding.
      "\xE0\x80\xAF",      // Overlong encoding.
      "\xF0\x80\x80\xAF",  // Overlong encoding.
      "\xED\xA0\x80",      // UTF-16 surrogate.
      "\xF4\x90\x80\x80",  // Above U+10FFFF.
  };
  for (const std::string_view invalid : kInvalid) {
    std::string_view text = invalid;
    EXPECT_THAT(ConsumeCodePoint(&text), Eq(std::nullopt));
    EXPECT_EQ(text, invalid);
  }
}

TEST(UnicodeTest, ConsumeCodePointInvalidUtf16LeavesStringUnchanged) {
  const std::u16string_view kInvalid[] = {
      u"\xD800",        // Truncated pair.
      u"\xD800z",       // Lead without trail.
      u"\xDC00z",       // Trail without lead.
      u"\xD800\xD800",  // Lead followed by lead.
  };
  for (const std::u16string_view invalid : kInvalid) {
    std::u16string_view text = invalid;
    EXPECT_THAT(ConsumeCodePoint(&text), Eq(std::nullopt));
    EXPECT_EQ(text, invalid);
  }
}

TEST(UnicodeTest, ConsumeCodePointStopsAtInvalid) {
  std::string_view u8_text = "a\xC3\xA9\xFF";
  EXPECT_THAT(ConsumeCodePoint(&u8_text), Optional(U'a'));
  EXPECT_THAT(ConsumeCodePoint(&u8_text), Optional(char32_t{0xE9}));
  EXPECT_THAT(ConsumeCodePoint(&u8_text), Eq(std::nullopt));
  EXPECT_EQ(u8_text, "\xFF");

  std::u16string_view u16_text = u"a\xD83D\xDE00\xDC00";
  EXPECT_THAT(ConsumeCodePoint(&u16_text), Optional(U'a'));
  EXPECT_THAT(ConsumeCodePoint(&u16_text), Optional(char32_t{0x1F600}));
  EXPECT_THAT(ConsumeCodePoint(&u16_text), Eq(std::nullopt));
  EXPECT_EQ(u16_text, u"\xDC00");
}

TEST(UnicodeTest, ConsumeCodePointKeepsByteOrderMark) {
  std::string_view u8_text = "\xEF\xBB\xBF";
  EXPECT_THAT(ConsumeCodePoint(&u8_text), Optional(char32_t{0xFEFF}));
  std::u16string_view u16_text = u"\xFEFF";
  EXPECT_THAT(ConsumeCodePoint(&u16_text), Optional(char32_t{0xFEFF}));
}

}  // namespace
}  // namespace gb
