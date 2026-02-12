// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/config/text_config.h"

#include "absl/strings/str_split.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

//==============================================================================
// WriteConfigToText — Scalar types
//==============================================================================

TEST(WriteConfigToTextTest, BoolTrue) {
  EXPECT_EQ(WriteConfigToText(Config::Bool(true)), "true");
}

TEST(WriteConfigToTextTest, BoolFalse) {
  EXPECT_EQ(WriteConfigToText(Config::Bool(false)), "false");
}

TEST(WriteConfigToTextTest, IntZero) {
  EXPECT_EQ(WriteConfigToText(Config::Int(0)), "0");
}

TEST(WriteConfigToTextTest, IntPositive) {
  EXPECT_EQ(WriteConfigToText(Config::Int(42)), "42");
}

TEST(WriteConfigToTextTest, IntNegative) {
  EXPECT_EQ(WriteConfigToText(Config::Int(-100)), "-100");
}

TEST(WriteConfigToTextTest, IntLargePositive) {
  EXPECT_EQ(WriteConfigToText(Config::Int(INT64_MAX)),
            std::to_string(INT64_MAX));
}

TEST(WriteConfigToTextTest, IntLargeNegative) {
  EXPECT_EQ(WriteConfigToText(Config::Int(INT64_MIN)),
            std::to_string(INT64_MIN));
}

TEST(WriteConfigToTextTest, Float) {
  std::string result = WriteConfigToText(Config::Float(3.14));
  EXPECT_THAT(result, HasSubstr("3.14"));
}

TEST(WriteConfigToTextTest, FloatZero) {
  EXPECT_EQ(WriteConfigToText(Config::Float(0.0)), "0");
}

TEST(WriteConfigToTextTest, FloatNegative) {
  std::string result = WriteConfigToText(Config::Float(-2.5));
  EXPECT_THAT(result, HasSubstr("-2.5"));
}

//==============================================================================
// WriteConfigToText — Strings
//==============================================================================

TEST(WriteConfigToTextTest, SimpleString) {
  EXPECT_EQ(WriteConfigToText(Config::String("hello")), "\"hello\"");
}

TEST(WriteConfigToTextTest, EmptyString) {
  EXPECT_EQ(WriteConfigToText(Config::String("")), "\"\"");
}

TEST(WriteConfigToTextTest, StringEscapeNewline) {
  EXPECT_EQ(WriteConfigToText(Config::String("a\nb")), "\"a\\nb\"");
}

TEST(WriteConfigToTextTest, StringEscapeTab) {
  EXPECT_EQ(WriteConfigToText(Config::String("a\tb")), "\"a\\tb\"");
}

TEST(WriteConfigToTextTest, StringEscapeBackslash) {
  EXPECT_EQ(WriteConfigToText(Config::String("a\\b")), "\"a\\\\b\"");
}

TEST(WriteConfigToTextTest, StringEscapeDoubleQuoteJson) {
  EXPECT_EQ(WriteConfigToText(Config::String("a\"b"), kJsonTextConfig),
            "\"a\\\"b\"");
}

TEST(WriteConfigToTextTest, StringEscapeControlChar) {
  std::string input(1, '\x01');
  EXPECT_EQ(WriteConfigToText(Config::String(input)), "\"\\x01\"");
}

TEST(WriteConfigToTextTest, StringEscapeControlCharBoundary) {
  std::string input(1, '\x1f');
  EXPECT_EQ(WriteConfigToText(Config::String(input)), "\"\\x1f\"");
}

TEST(WriteConfigToTextTest, StringEscapeNullByte) {
  std::string input(1, '\x00');
  EXPECT_EQ(WriteConfigToText(Config::String(input)), "\"\\x00\"");
}

TEST(WriteConfigToTextTest, StringPrintableCharPassthrough) {
  EXPECT_EQ(WriteConfigToText(Config::String(" ")), "\" \"");
  EXPECT_EQ(WriteConfigToText(Config::String("~")), "\"~\"");
}

TEST(WriteConfigToTextTest, StringMultipleEscapes) {
  EXPECT_EQ(WriteConfigToText(Config::String("a\nb\tc\\")),
            "\"a\\nb\\tc\\\\\"");
}

// Single quote flag: string with only double quotes uses single quotes.
TEST(WriteConfigToTextTest, StringSingleQuoteFlagWithDoubleQuote) {
  EXPECT_EQ(WriteConfigToText(Config::String("a\"b"), kDefaultTextConfig),
            "'a\"b'");
}

// Single quote flag: string with both quote types stays double-quoted.
TEST(WriteConfigToTextTest, StringSingleQuoteFlagWithBothQuotes) {
  EXPECT_EQ(WriteConfigToText(Config::String("a\"b'c"), kDefaultTextConfig),
            "\"a\\\"b'c\"");
}

// Single quote flag: string with only single quotes stays double-quoted, and
// the single quote is not escaped since double quotes are the enclosing style.
TEST(WriteConfigToTextTest, StringSingleQuoteFlagWithOnlySingleQuote) {
  EXPECT_EQ(WriteConfigToText(Config::String("a'b"), kDefaultTextConfig),
            "\"a'b\"");
}

// Single quote flag: string with no quotes uses double quotes.
TEST(WriteConfigToTextTest, StringSingleQuoteFlagWithNoQuotes) {
  EXPECT_EQ(WriteConfigToText(Config::String("abc"), kDefaultTextConfig),
            "\"abc\"");
}

// No single quote flag: double quotes must be escaped.
TEST(WriteConfigToTextTest, StringNoSingleQuoteFlag) {
  EXPECT_EQ(WriteConfigToText(Config::String("a\"b"), kJsonTextConfig),
            "\"a\\\"b\"");
}

// Single quote as enclosing style: escape sequences still work.
TEST(WriteConfigToTextTest, StringSingleQuotedWithEscapes) {
  EXPECT_EQ(WriteConfigToText(Config::String("a\n\"b"), kDefaultTextConfig),
            "'a\\n\"b'");
}

//==============================================================================
// WriteConfigToText — Empty map and array
//==============================================================================

TEST(WriteConfigToTextTest, EmptyMap) {
  Config config = Config::Map();
  EXPECT_EQ(WriteConfigToText(config), "{\n\n}");
}

TEST(WriteConfigToTextTest, EmptyArray) {
  Config config = Config::Array();
  EXPECT_EQ(WriteConfigToText(config), "[]");
}

//==============================================================================
// WriteConfigToText — Map with identifier keys (default flags)
//==============================================================================

TEST(WriteConfigToTextTest, MapSingleKeyIdentifier) {
  Config config = Config::Map();
  config.Set("key", Config::Int(1));
  EXPECT_EQ(WriteConfigToText(config), "{\n  key: 1\n}");
}

TEST(WriteConfigToTextTest, MapMultipleKeysAreSorted) {
  Config config = Config::Map();
  config.Set("b", Config::Int(2));
  config.Set("a", Config::Int(1));
  config.Set("c", Config::Int(3));
  EXPECT_EQ(WriteConfigToText(config), "{\n  a: 1,\n  b: 2,\n  c: 3\n}");
}

TEST(WriteConfigToTextTest, MapKeyStartingWithDigitNotIdentifier) {
  Config config = Config::Map();
  config.Set("123", Config::Int(1));
  EXPECT_EQ(WriteConfigToText(config), "{\n  \"123\": 1\n}");
}

TEST(WriteConfigToTextTest, MapKeyWithSpaceNotIdentifier) {
  Config config = Config::Map();
  config.Set("a b", Config::Int(1));
  EXPECT_EQ(WriteConfigToText(config), "{\n  \"a b\": 1\n}");
}

TEST(WriteConfigToTextTest, MapKeyEmptyStringNotIdentifier) {
  Config config = Config::Map();
  config.Set("", Config::Int(1));
  EXPECT_EQ(WriteConfigToText(config), "{\n  \"\": 1\n}");
}

TEST(WriteConfigToTextTest, MapKeyWithHyphenNotIdentifier) {
  Config config = Config::Map();
  config.Set("a-b", Config::Int(1));
  EXPECT_EQ(WriteConfigToText(config), "{\n  \"a-b\": 1\n}");
}

TEST(WriteConfigToTextTest, MapKeyIdentifierWithLeadingUnderscore) {
  Config config = Config::Map();
  config.Set("_foo", Config::Int(1));
  EXPECT_EQ(WriteConfigToText(config), "{\n  _foo: 1\n}");
}

TEST(WriteConfigToTextTest, MapKeySingleUnderscore) {
  Config config = Config::Map();
  config.Set("_", Config::Int(1));
  EXPECT_EQ(WriteConfigToText(config), "{\n  _: 1\n}");
}

TEST(WriteConfigToTextTest, MapKeyIdentifierWithTrailingDigits) {
  Config config = Config::Map();
  config.Set("item2", Config::Int(1));
  EXPECT_EQ(WriteConfigToText(config), "{\n  item2: 1\n}");
}

TEST(WriteConfigToTextTest, MapKeyNoIdentifierFlag) {
  Config config = Config::Map();
  config.Set("key", Config::Int(1));
  EXPECT_EQ(WriteConfigToText(config, kJsonTextConfig), "{\n  \"key\": 1\n}");
}

//==============================================================================
// WriteConfigToText — Map with all value types
//==============================================================================

TEST(WriteConfigToTextTest, MapWithAllValueTypes) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(10));
  Config inner = Config::Map();
  inner.Set("nested", Config::Bool(false));
  Config config = Config::Map();
  config.Set("a", Config::Bool(true));
  config.Set("b", Config::Int(42));
  config.Set("c", Config::Float(1.5));
  config.Set("d", Config::String("hello"));
  config.Set("e", Config::Array(std::move(arr)));
  config.Set("f", std::move(inner));
  std::string result = WriteConfigToText(config);
  EXPECT_THAT(result, HasSubstr("a: true"));
  EXPECT_THAT(result, HasSubstr("b: 42"));
  EXPECT_THAT(result, HasSubstr("d: \"hello\""));
  EXPECT_THAT(result, HasSubstr("e: [10]"));
  EXPECT_THAT(result, HasSubstr("f: {\n"));
  EXPECT_THAT(result, HasSubstr("nested: false"));
}

//==============================================================================
// WriteConfigToText — Nested maps with indentation
//==============================================================================

TEST(WriteConfigToTextTest, NestedMap) {
  Config inner = Config::Map();
  inner.Set("c", Config::Int(3));
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  config.Set("b", std::move(inner));
  EXPECT_EQ(WriteConfigToText(config),
            "{\n"
            "  a: 1,\n"
            "  b: {\n"
            "    c: 3\n"
            "  }\n"
            "}");
}

TEST(WriteConfigToTextTest, DeeplyNestedMap) {
  Config inner2 = Config::Map();
  inner2.Set("z", Config::Int(99));
  Config inner1 = Config::Map();
  inner1.Set("y", std::move(inner2));
  Config config = Config::Map();
  config.Set("x", std::move(inner1));
  EXPECT_EQ(WriteConfigToText(config),
            "{\n"
            "  x: {\n"
            "    y: {\n"
            "      z: 99\n"
            "    }\n"
            "  }\n"
            "}");
}

//==============================================================================
// WriteConfigToText — Array values
//==============================================================================

TEST(WriteConfigToTextTest, SingleElementArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(42));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(WriteConfigToText(config), "[42]");
}

TEST(WriteConfigToTextTest, SimpleArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  arr.push_back(Config::Int(3));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(WriteConfigToText(config), "[1, 2, 3]");
}

TEST(WriteConfigToTextTest, ArrayWithMixedTypes) {
  Config::ArrayValue arr;
  arr.push_back(Config::Bool(true));
  arr.push_back(Config::Int(42));
  arr.push_back(Config::String("hi"));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(WriteConfigToText(config), "[true, 42, \"hi\"]");
}

TEST(WriteConfigToTextTest, ArrayWithNestedMap) {
  Config inner = Config::Map();
  inner.Set("a", Config::Int(1));
  Config::ArrayValue arr;
  arr.push_back(std::move(inner));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(WriteConfigToText(config),
            "[{\n"
            "    a: 1\n"
            "  }]");
}

TEST(WriteConfigToTextTest, ArrayWithNestedArray) {
  Config::ArrayValue inner_arr;
  inner_arr.push_back(Config::Int(1));
  inner_arr.push_back(Config::Int(2));
  Config::ArrayValue arr;
  arr.push_back(Config::Array(std::move(inner_arr)));
  arr.push_back(Config::Int(3));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(WriteConfigToText(config), "[[1, 2], 3]");
}

TEST(WriteConfigToTextTest, MapContainingArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  Config config = Config::Map();
  config.Set("list", Config::Array(std::move(arr)));
  EXPECT_EQ(WriteConfigToText(config),
            "{\n"
            "  list: [1, 2]\n"
            "}");
}

//==============================================================================
// WriteConfigToText — Array line wrapping
//==============================================================================

TEST(WriteConfigToTextTest, ArrayShortNoWrapping) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  arr.push_back(Config::Int(3));
  Config config = Config::Array(std::move(arr));
  std::string result = WriteConfigToText(config);
  EXPECT_THAT(result, Not(HasSubstr("\n")));
}

TEST(WriteConfigToTextTest, ArrayLineWrapping) {
  Config::ArrayValue arr;
  for (int i = 0; i < 40; ++i) {
    arr.push_back(Config::Int(1000000 + i));
  }
  Config config = Config::Array(std::move(arr));
  std::string result = WriteConfigToText(config);
  EXPECT_EQ(result.front(), '[');
  EXPECT_EQ(result.back(), ']');
  EXPECT_THAT(result, HasSubstr("\n"));
}

TEST(WriteConfigToTextTest, ArrayLineWrappingRespectsLineLength) {
  Config::ArrayValue arr;
  for (int i = 0; i < 40; ++i) {
    arr.push_back(Config::Int(1000000 + i));
  }
  Config config = Config::Array(std::move(arr));
  std::string result = WriteConfigToText(config);
  for (absl::string_view line : absl::StrSplit(result, '\n')) {
    EXPECT_LE(line.size(), kTextConfigMaxArrayLineLength + 20)
        << "Line too long: " << line;
  }
}

TEST(WriteConfigToTextTest, ArrayWrappingInMap) {
  Config::ArrayValue arr;
  for (int i = 0; i < 30; ++i) {
    arr.push_back(Config::Int(1000000 + i));
  }
  Config config = Config::Map();
  config.Set("data", Config::Array(std::move(arr)));
  std::string result = WriteConfigToText(config);
  EXPECT_THAT(result, HasSubstr("data: ["));
  EXPECT_THAT(result, HasSubstr("\n"));
}

//==============================================================================
// WriteConfigToText — Compact output
//==============================================================================

TEST(WriteConfigToTextTest, CompactBool) {
  EXPECT_EQ(WriteConfigToText(Config::Bool(true), kCompactTextConfig), "true");
}

TEST(WriteConfigToTextTest, CompactInt) {
  EXPECT_EQ(WriteConfigToText(Config::Int(42), kCompactTextConfig), "42");
}

TEST(WriteConfigToTextTest, CompactString) {
  EXPECT_EQ(WriteConfigToText(Config::String("hi"), kCompactTextConfig),
            "\"hi\"");
}

TEST(WriteConfigToTextTest, CompactEmptyMap) {
  Config config = Config::Map();
  EXPECT_EQ(WriteConfigToText(config, kCompactTextConfig), "{}");
}

TEST(WriteConfigToTextTest, CompactMap) {
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  config.Set("b", Config::Int(2));
  EXPECT_EQ(WriteConfigToText(config, kCompactTextConfig), "{a:1,b:2}");
}

TEST(WriteConfigToTextTest, CompactEmptyArray) {
  Config config = Config::Array();
  EXPECT_EQ(WriteConfigToText(config, kCompactTextConfig), "[]");
}

TEST(WriteConfigToTextTest, CompactArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  arr.push_back(Config::Int(3));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(WriteConfigToText(config, kCompactTextConfig), "[1,2,3]");
}

TEST(WriteConfigToTextTest, CompactNestedMap) {
  Config inner = Config::Map();
  inner.Set("b", Config::Int(2));
  Config config = Config::Map();
  config.Set("a", std::move(inner));
  EXPECT_EQ(WriteConfigToText(config, kCompactTextConfig), "{a:{b:2}}");
}

TEST(WriteConfigToTextTest, CompactNestedArray) {
  Config::ArrayValue inner;
  inner.push_back(Config::Int(1));
  Config::ArrayValue outer;
  outer.push_back(Config::Array(std::move(inner)));
  Config config = Config::Array(std::move(outer));
  EXPECT_EQ(WriteConfigToText(config, kCompactTextConfig), "[[1]]");
}

TEST(WriteConfigToTextTest, CompactMapWithArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  Config config = Config::Map();
  config.Set("x", Config::Array(std::move(arr)));
  EXPECT_EQ(WriteConfigToText(config, kCompactTextConfig), "{x:[1,2]}");
}

TEST(WriteConfigToTextTest, CompactMapNoWhitespace) {
  Config config = Config::Map();
  config.Set("key", Config::String("value"));
  std::string result = WriteConfigToText(config, kCompactTextConfig);
  EXPECT_EQ(result.find(' '), std::string::npos);
  EXPECT_EQ(result.find('\n'), std::string::npos);
}

TEST(WriteConfigToTextTest, CompactArrayWithMaps) {
  Config m1 = Config::Map();
  m1.Set("a", Config::Int(1));
  Config m2 = Config::Map();
  m2.Set("b", Config::Int(2));
  Config::ArrayValue arr;
  arr.push_back(std::move(m1));
  arr.push_back(std::move(m2));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(WriteConfigToText(config, kCompactTextConfig), "[{a:1},{b:2}]");
}

TEST(WriteConfigToTextTest, CompactLargeArrayNoWrapping) {
  Config::ArrayValue arr;
  for (int i = 0; i < 40; ++i) {
    arr.push_back(Config::Int(i));
  }
  Config config = Config::Array(std::move(arr));
  std::string result = WriteConfigToText(config, kCompactTextConfig);
  EXPECT_EQ(result.find('\n'), std::string::npos);
}

//==============================================================================
// WriteConfigToText — Rootless mode
//==============================================================================

TEST(WriteConfigToTextTest, RootlessMap) {
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  config.Set("b", Config::Int(2));
  EXPECT_EQ(WriteConfigToText(config, kRootlessTextConfig), "a: 1,\nb: 2");
}

TEST(WriteConfigToTextTest, RootlessEmptyMap) {
  Config config = Config::Map();
  EXPECT_EQ(WriteConfigToText(config, kRootlessTextConfig), "");
}

TEST(WriteConfigToTextTest, RootlessNonMapReturnsEmpty) {
  EXPECT_EQ(WriteConfigToText(Config::Int(42), kRootlessTextConfig), "");
  EXPECT_EQ(WriteConfigToText(Config::Bool(true), kRootlessTextConfig), "");
  EXPECT_EQ(WriteConfigToText(Config::String("hi"), kRootlessTextConfig), "");
  EXPECT_EQ(WriteConfigToText(Config::Float(1.0), kRootlessTextConfig), "");
  Config arr = Config::Array();
  EXPECT_EQ(WriteConfigToText(arr, kRootlessTextConfig), "");
}

TEST(WriteConfigToTextTest, RootlessMapWithNestedMap) {
  Config inner = Config::Map();
  inner.Set("c", Config::Int(3));
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  config.Set("b", std::move(inner));
  EXPECT_EQ(WriteConfigToText(config, kRootlessTextConfig),
            "a: 1,\n"
            "b: {\n"
            "  c: 3\n"
            "}");
}

TEST(WriteConfigToTextTest, RootlessMapWithDeeplyNestedMap) {
  Config inner2 = Config::Map();
  inner2.Set("z", Config::Int(99));
  Config inner1 = Config::Map();
  inner1.Set("y", std::move(inner2));
  Config config = Config::Map();
  config.Set("x", std::move(inner1));
  EXPECT_EQ(WriteConfigToText(config, kRootlessTextConfig),
            "x: {\n"
            "  y: {\n"
            "    z: 99\n"
            "  }\n"
            "}");
}

TEST(WriteConfigToTextTest, RootlessMapWithArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  Config config = Config::Map();
  config.Set("x", Config::Array(std::move(arr)));
  EXPECT_EQ(WriteConfigToText(config, kRootlessTextConfig), "x: [1, 2]");
}

TEST(WriteConfigToTextTest, RootlessMapKeysNotIndented) {
  Config config = Config::Map();
  config.Set("key", Config::Int(1));
  std::string result = WriteConfigToText(config, kRootlessTextConfig);
  EXPECT_EQ(result[0], 'k');
}

//==============================================================================
// WriteConfigToText — JSON-style flags (no identifiers, no single quotes)
//==============================================================================

TEST(WriteConfigToTextTest, JsonMap) {
  Config config = Config::Map();
  config.Set("key", Config::Int(1));
  EXPECT_EQ(WriteConfigToText(config, kJsonTextConfig), "{\n  \"key\": 1\n}");
}

TEST(WriteConfigToTextTest, JsonString) {
  EXPECT_EQ(WriteConfigToText(Config::String("hello"), kJsonTextConfig),
            "\"hello\"");
}

TEST(WriteConfigToTextTest, JsonStringWithDoubleQuote) {
  EXPECT_EQ(WriteConfigToText(Config::String("a\"b"), kJsonTextConfig),
            "\"a\\\"b\"");
}

TEST(WriteConfigToTextTest, JsonNestedMap) {
  Config inner = Config::Map();
  inner.Set("b", Config::Int(2));
  Config config = Config::Map();
  config.Set("a", std::move(inner));
  EXPECT_EQ(WriteConfigToText(config, kJsonTextConfig),
            "{\n"
            "  \"a\": {\n"
            "    \"b\": 2\n"
            "  }\n"
            "}");
}

//==============================================================================
// WriteConfigToText — Compact + Rootless combination
//==============================================================================

TEST(WriteConfigToTextTest, CompactRootlessMap) {
  TextConfigFlags flags = {kDefaultTextConfig, TextConfigFlag::kCompact,
                           TextConfigFlag::kRootless};
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  config.Set("b", Config::Int(2));
  EXPECT_EQ(WriteConfigToText(config, flags), "a:1,b:2");
}

TEST(WriteConfigToTextTest, CompactRootlessNonMapReturnsEmpty) {
  TextConfigFlags flags = {kDefaultTextConfig, TextConfigFlag::kCompact,
                           TextConfigFlag::kRootless};
  EXPECT_EQ(WriteConfigToText(Config::Int(42), flags), "");
}

TEST(WriteConfigToTextTest, CompactRootlessEmptyMap) {
  TextConfigFlags flags = {kDefaultTextConfig, TextConfigFlag::kCompact,
                           TextConfigFlag::kRootless};
  Config config = Config::Map();
  EXPECT_EQ(WriteConfigToText(config, flags), "");
}

//==============================================================================
// WriteConfigToText — Predefined flag constants
//==============================================================================

TEST(WriteConfigToTextTest, DefaultFlagsHaveIdentifiersAndSingleQuotes) {
  EXPECT_TRUE(kDefaultTextConfig.IsSet(TextConfigFlag::kIdentifiers));
  EXPECT_TRUE(kDefaultTextConfig.IsSet(TextConfigFlag::kSingleQuotes));
  EXPECT_TRUE(kDefaultTextConfig.IsSet(TextConfigFlag::kComments));
  EXPECT_FALSE(kDefaultTextConfig.IsSet(TextConfigFlag::kCompact));
  EXPECT_FALSE(kDefaultTextConfig.IsSet(TextConfigFlag::kRootless));
}

TEST(WriteConfigToTextTest, CompactFlagsIncludeCompact) {
  EXPECT_TRUE(kCompactTextConfig.IsSet(TextConfigFlag::kCompact));
  EXPECT_TRUE(kCompactTextConfig.IsSet(TextConfigFlag::kIdentifiers));
  EXPECT_TRUE(kCompactTextConfig.IsSet(TextConfigFlag::kSingleQuotes));
  EXPECT_TRUE(kCompactTextConfig.IsSet(TextConfigFlag::kComments));
}

TEST(WriteConfigToTextTest, RootlessFlagsIncludeRootless) {
  EXPECT_TRUE(kRootlessTextConfig.IsSet(TextConfigFlag::kRootless));
  EXPECT_TRUE(kRootlessTextConfig.IsSet(TextConfigFlag::kIdentifiers));
  EXPECT_TRUE(kRootlessTextConfig.IsSet(TextConfigFlag::kSingleQuotes));
  EXPECT_TRUE(kRootlessTextConfig.IsSet(TextConfigFlag::kComments));
}

TEST(WriteConfigToTextTest, JsonFlagsAreEmpty) {
  EXPECT_TRUE(kJsonTextConfig.IsEmpty());
}

//==============================================================================
// ReadConfigFromText — Unimplemented
//==============================================================================

TEST(ReadConfigFromTextTest, ReturnsUnimplementedError) {
  auto result = ReadConfigFromText("true");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kUnimplemented);
}

}  // namespace
}  // namespace gb
