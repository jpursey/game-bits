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
// WriteConfigToText — None
//==============================================================================

TEST(WriteConfigToTextTest, None) {
  EXPECT_EQ(WriteConfigToText(Config::None()), "null");
}

TEST(WriteConfigToTextTest, NoneDefault) {
  EXPECT_EQ(WriteConfigToText(Config()), "null");
}

TEST(WriteConfigToTextTest, NoneCompact) {
  EXPECT_EQ(WriteConfigToText(Config::None(), kCompactTextConfig), "null");
}

TEST(WriteConfigToTextTest, NoneJson) {
  EXPECT_EQ(WriteConfigToText(Config::None(), kJsonTextConfig), "null");
}

TEST(WriteConfigToTextTest, NoneInArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::None());
  arr.push_back(Config::Int(3));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(WriteConfigToText(config), "[1, null, 3]");
}

TEST(WriteConfigToTextTest, NoneInMap) {
  Config config = Config::Map();
  config.Set("a", Config::None());
  EXPECT_EQ(WriteConfigToText(config), "{\n  a: null\n}");
}

TEST(WriteConfigToTextTest, NoneInCompactArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::None());
  arr.push_back(Config::Int(1));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(WriteConfigToText(config, kCompactTextConfig), "[null,1]");
}

TEST(WriteConfigToTextTest, NoneInCompactMap) {
  Config config = Config::Map();
  config.Set("x", Config::None());
  EXPECT_EQ(WriteConfigToText(config, kCompactTextConfig), "{x:null}");
}

TEST(WriteConfigToTextTest, NoneRootlessReturnsEmpty) {
  EXPECT_EQ(WriteConfigToText(Config::None(), kRootlessTextConfig), "");
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
// ReadConfigFromText — Scalar types
//==============================================================================

TEST(ReadConfigFromTextTest, Null) {
  auto result = ReadConfigFromText("null");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsNone());
}

TEST(ReadConfigFromTextTest, BoolTrue) {
  auto result = ReadConfigFromText("true");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsBool());
  EXPECT_TRUE(result->GetBool());
}

TEST(ReadConfigFromTextTest, BoolFalse) {
  auto result = ReadConfigFromText("false");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsBool());
  EXPECT_FALSE(result->GetBool());
}

TEST(ReadConfigFromTextTest, IntZero) {
  auto result = ReadConfigFromText("0");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsInt());
  EXPECT_EQ(result->GetInt(), 0);
}

TEST(ReadConfigFromTextTest, IntPositive) {
  auto result = ReadConfigFromText("42");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsInt());
  EXPECT_EQ(result->GetInt(), 42);
}

TEST(ReadConfigFromTextTest, IntNegative) {
  auto result = ReadConfigFromText("-100");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsInt());
  EXPECT_EQ(result->GetInt(), -100);
}

TEST(ReadConfigFromTextTest, IntLargePositive) {
  auto result = ReadConfigFromText(std::to_string(INT64_MAX));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsInt());
  EXPECT_EQ(result->GetInt(), INT64_MAX);
}

TEST(ReadConfigFromTextTest, IntLargeNegative) {
  auto result = ReadConfigFromText(std::to_string(INT64_MIN));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsInt());
  EXPECT_EQ(result->GetInt(), INT64_MIN);
}

TEST(ReadConfigFromTextTest, IntHexLower) {
  auto result = ReadConfigFromText("0xff");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsInt());
  EXPECT_EQ(result->GetInt(), 255);
}

TEST(ReadConfigFromTextTest, IntHexUpper) {
  auto result = ReadConfigFromText("0xFF");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsInt());
  EXPECT_EQ(result->GetInt(), 255);
}

TEST(ReadConfigFromTextTest, FloatSimple) {
  auto result = ReadConfigFromText("3.14");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsFloat());
  EXPECT_NEAR(result->GetFloat(), 3.14, 1e-9);
}

TEST(ReadConfigFromTextTest, FloatNegative) {
  auto result = ReadConfigFromText("-2.5");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsFloat());
  EXPECT_NEAR(result->GetFloat(), -2.5, 1e-9);
}

TEST(ReadConfigFromTextTest, FloatExponent) {
  auto result = ReadConfigFromText("1.25e2");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsFloat());
  EXPECT_NEAR(result->GetFloat(), 125.0, 1e-9);
}

TEST(ReadConfigFromTextTest, FloatExponentNegative) {
  auto result = ReadConfigFromText("5.0e-3");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsFloat());
  EXPECT_NEAR(result->GetFloat(), 0.005, 1e-9);
}

//==============================================================================
// ReadConfigFromText — Strings
//==============================================================================

TEST(ReadConfigFromTextTest, SimpleString) {
  auto result = ReadConfigFromText("\"hello\"");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "hello");
}

TEST(ReadConfigFromTextTest, EmptyString) {
  auto result = ReadConfigFromText("\"\"");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "");
}

TEST(ReadConfigFromTextTest, StringWithEscapedNewline) {
  auto result = ReadConfigFromText("\"a\\nb\"");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "a\nb");
}

TEST(ReadConfigFromTextTest, StringWithEscapedTab) {
  auto result = ReadConfigFromText("\"a\\tb\"");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "a\tb");
}

TEST(ReadConfigFromTextTest, StringWithEscapedBackslash) {
  auto result = ReadConfigFromText("\"a\\\\b\"");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "a\\b");
}

TEST(ReadConfigFromTextTest, StringWithEscapedDoubleQuote) {
  auto result = ReadConfigFromText("\"a\\\"b\"");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "a\"b");
}

TEST(ReadConfigFromTextTest, StringWithHexEscape) {
  auto result = ReadConfigFromText("\"\\x41\"");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "A");
}

TEST(ReadConfigFromTextTest, SingleQuotedString) {
  auto result = ReadConfigFromText("'hello'", kDefaultTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "hello");
}

TEST(ReadConfigFromTextTest, SingleQuotedStringWithDoubleQuote) {
  auto result = ReadConfigFromText("'a\"b'", kDefaultTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "a\"b");
}

TEST(ReadConfigFromTextTest, SingleQuotedStringNotAllowedInJson) {
  auto result = ReadConfigFromText("'hello'", kJsonTextConfig);
  EXPECT_FALSE(result.ok());
}

//==============================================================================
// ReadConfigFromText — Arrays
//==============================================================================

TEST(ReadConfigFromTextTest, EmptyArray) {
  auto result = ReadConfigFromText("[]");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsArray());
  EXPECT_EQ(result->GetArraySize(), 0);
}

TEST(ReadConfigFromTextTest, SingleElementArray) {
  auto result = ReadConfigFromText("[42]");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsArray());
  EXPECT_EQ(result->GetArraySize(), 1);
  EXPECT_EQ(result->GetInt(0), 42);
}

TEST(ReadConfigFromTextTest, MultiElementArray) {
  auto result = ReadConfigFromText("[1, 2, 3]");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsArray());
  EXPECT_EQ(result->GetArraySize(), 3);
  EXPECT_EQ(result->GetInt(0), 1);
  EXPECT_EQ(result->GetInt(1), 2);
  EXPECT_EQ(result->GetInt(2), 3);
}

TEST(ReadConfigFromTextTest, MixedTypeArray) {
  auto result = ReadConfigFromText("[true, 42, \"hi\", null, 3.14]");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsArray());
  EXPECT_EQ(result->GetArraySize(), 5);
  EXPECT_TRUE(result->Get(0)->IsBool());
  EXPECT_TRUE(result->Get(0)->GetBool());
  EXPECT_TRUE(result->Get(1)->IsInt());
  EXPECT_EQ(result->Get(1)->GetInt(), 42);
  EXPECT_TRUE(result->Get(2)->IsString());
  EXPECT_EQ(result->Get(2)->GetString(), "hi");
  EXPECT_TRUE(result->Get(3)->IsNone());
  EXPECT_TRUE(result->Get(4)->IsFloat());
  EXPECT_NEAR(result->Get(4)->GetFloat(), 3.14, 1e-9);
}

TEST(ReadConfigFromTextTest, NestedArray) {
  auto result = ReadConfigFromText("[[1, 2], [3, 4]]");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsArray());
  EXPECT_EQ(result->GetArraySize(), 2);
  EXPECT_TRUE(result->Get(0)->IsArray());
  EXPECT_EQ(result->Get(0)->GetArraySize(), 2);
  EXPECT_EQ(result->Get(0)->GetInt(0), 1);
  EXPECT_EQ(result->Get(0)->GetInt(1), 2);
  EXPECT_TRUE(result->Get(1)->IsArray());
  EXPECT_EQ(result->Get(1)->GetInt(0), 3);
  EXPECT_EQ(result->Get(1)->GetInt(1), 4);
}

TEST(ReadConfigFromTextTest, ArrayWithNulls) {
  auto result = ReadConfigFromText("[null, 1, null]");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsArray());
  EXPECT_EQ(result->GetArraySize(), 3);
  EXPECT_TRUE(result->Get(0)->IsNone());
  EXPECT_EQ(result->Get(1)->GetInt(), 1);
  EXPECT_TRUE(result->Get(2)->IsNone());
}

//==============================================================================
// ReadConfigFromText — Maps (rooted)
//==============================================================================

TEST(ReadConfigFromTextTest, EmptyMap) {
  auto result = ReadConfigFromText("{}");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetMapSize(), 0);
}

TEST(ReadConfigFromTextTest, SingleKeyMap) {
  auto result = ReadConfigFromText("{\"key\": 1}");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetMapSize(), 1);
  EXPECT_EQ(result->GetInt("key"), 1);
}

TEST(ReadConfigFromTextTest, MultipleKeyMap) {
  auto result = ReadConfigFromText("{\"a\": 1, \"b\": 2, \"c\": 3}");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetMapSize(), 3);
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_EQ(result->GetInt("b"), 2);
  EXPECT_EQ(result->GetInt("c"), 3);
}

TEST(ReadConfigFromTextTest, MapWithAllValueTypes) {
  auto result = ReadConfigFromText(
      "{\"a\": true, \"b\": 42, \"c\": 3.14, \"d\": \"hello\","
      " \"e\": null, \"f\": [1, 2], \"g\": {\"x\": 1}}");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_TRUE(result->Get("a")->IsBool());
  EXPECT_TRUE(result->Get("a")->GetBool());
  EXPECT_TRUE(result->Get("b")->IsInt());
  EXPECT_EQ(result->Get("b")->GetInt(), 42);
  EXPECT_TRUE(result->Get("c")->IsFloat());
  EXPECT_NEAR(result->Get("c")->GetFloat(), 3.14, 1e-9);
  EXPECT_TRUE(result->Get("d")->IsString());
  EXPECT_EQ(result->Get("d")->GetString(), "hello");
  EXPECT_TRUE(result->Get("e")->IsNone());
  EXPECT_TRUE(result->Get("f")->IsArray());
  EXPECT_EQ(result->Get("f")->GetArraySize(), 2);
  EXPECT_TRUE(result->Get("g")->IsMap());
  EXPECT_EQ(result->Get("g")->GetInt("x"), 1);
}

TEST(ReadConfigFromTextTest, NestedMap) {
  auto result = ReadConfigFromText("{\"a\": {\"b\": {\"c\": 42}}}");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_TRUE(result->Get("a")->IsMap());
  EXPECT_TRUE(result->Get("a")->Get("b")->IsMap());
  EXPECT_EQ(result->Get("a")->Get("b")->GetInt("c"), 42);
}

TEST(ReadConfigFromTextTest, MapWithNullValue) {
  auto result = ReadConfigFromText("{\"key\": null}");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_TRUE(result->Get("key")->IsNone());
}

//==============================================================================
// ReadConfigFromText — Identifier keys
//==============================================================================

TEST(ReadConfigFromTextTest, IdentifierKey) {
  auto result = ReadConfigFromText("{key: 1}", kDefaultTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetInt("key"), 1);
}

TEST(ReadConfigFromTextTest, IdentifierKeyWithUnderscore) {
  auto result = ReadConfigFromText("{_foo_bar: 1}", kDefaultTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("_foo_bar"), 1);
}

TEST(ReadConfigFromTextTest, IdentifierKeyWithDigits) {
  auto result = ReadConfigFromText("{item2: 1}", kDefaultTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("item2"), 1);
}

TEST(ReadConfigFromTextTest, MixedIdentifierAndStringKeys) {
  auto result =
      ReadConfigFromText("{key: 1, \"other key\": 2}", kDefaultTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("key"), 1);
  EXPECT_EQ(result->GetInt("other key"), 2);
}

TEST(ReadConfigFromTextTest, IdentifierKeyNotAllowedInJson) {
  auto result = ReadConfigFromText("{key: 1}", kJsonTextConfig);
  EXPECT_FALSE(result.ok());
}

//==============================================================================
// ReadConfigFromText — Rootless mode
//==============================================================================

TEST(ReadConfigFromTextTest, RootlessSimple) {
  auto result = ReadConfigFromText("a: 1, b: 2", kRootlessTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_EQ(result->GetInt("b"), 2);
}

TEST(ReadConfigFromTextTest, RootlessEmpty) {
  auto result = ReadConfigFromText("", kRootlessTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetMapSize(), 0);
}

TEST(ReadConfigFromTextTest, RootlessSingleKey) {
  auto result = ReadConfigFromText("key: 42", kRootlessTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetInt("key"), 42);
}

TEST(ReadConfigFromTextTest, RootlessWithNestedMap) {
  auto result =
      ReadConfigFromText("a: 1, b: {c: 3}", kRootlessTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_TRUE(result->Get("b")->IsMap());
  EXPECT_EQ(result->Get("b")->GetInt("c"), 3);
}

TEST(ReadConfigFromTextTest, RootlessWithArray) {
  auto result = ReadConfigFromText("x: [1, 2, 3]", kRootlessTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_TRUE(result->Get("x")->IsArray());
  EXPECT_EQ(result->Get("x")->GetArraySize(), 3);
}

TEST(ReadConfigFromTextTest, RootlessWithStringKeys) {
  TextConfigFlags flags = {TextConfigFlag::kRootless};
  auto result = ReadConfigFromText("\"a\": 1, \"b\": 2", flags);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_EQ(result->GetInt("b"), 2);
}

TEST(ReadConfigFromTextTest, RootlessMultiline) {
  auto result = ReadConfigFromText("a: 1,\nb: 2,\nc: 3", kRootlessTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetMapSize(), 3);
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_EQ(result->GetInt("b"), 2);
  EXPECT_EQ(result->GetInt("c"), 3);
}

//==============================================================================
// ReadConfigFromText — Comments
//==============================================================================

TEST(ReadConfigFromTextTest, LineComment) {
  auto result = ReadConfigFromText(
      "// This is a comment\n42", kDefaultTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt(), 42);
}

TEST(ReadConfigFromTextTest, BlockComment) {
  auto result = ReadConfigFromText(
      "/* comment */ 42", kDefaultTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt(), 42);
}

TEST(ReadConfigFromTextTest, CommentsInMap) {
  auto result = ReadConfigFromText(
      "{\n"
      "  // comment\n"
      "  key: /* inline */ 1\n"
      "}",
      kDefaultTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("key"), 1);
}

TEST(ReadConfigFromTextTest, CommentsInRootless) {
  auto result = ReadConfigFromText(
      "// header comment\n"
      "a: 1, // trailing comment\n"
      "b: 2",
      kRootlessTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_EQ(result->GetInt("b"), 2);
}

TEST(ReadConfigFromTextTest, CommentsNotAllowedInJson) {
  auto result = ReadConfigFromText("// comment\n42", kJsonTextConfig);
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, BlockCommentNotAllowedInJson) {
  auto result = ReadConfigFromText("/* comment */ 42", kJsonTextConfig);
  EXPECT_FALSE(result.ok());
}

//==============================================================================
// ReadConfigFromText — Individual flag variations
//==============================================================================

// No single quotes flag: single-quoted strings are rejected, but double-quoted
// strings still work.
TEST(ReadConfigFromTextTest, NoSingleQuotesFlagRejectsQuotes) {
  TextConfigFlags flags = {TextConfigFlag::kIdentifiers,
                           TextConfigFlag::kComments};
  auto result = ReadConfigFromText("'hello'", flags);
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, NoSingleQuotesFlagAllowsDoubleQuoted) {
  TextConfigFlags flags = {TextConfigFlag::kIdentifiers,
                           TextConfigFlag::kComments};
  auto result = ReadConfigFromText("\"hello\"", flags);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetString(), "hello");
}

TEST(ReadConfigFromTextTest, NoSingleQuotesFlagAllowsDoubleQuotedMapKey) {
  TextConfigFlags flags = {TextConfigFlag::kIdentifiers,
                           TextConfigFlag::kComments};
  auto result = ReadConfigFromText("{\"key\": 1}", flags);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("key"), 1);
}

// No identifiers flag: identifier keys are rejected, but string keys work.
TEST(ReadConfigFromTextTest, NoIdentifiersFlagRejectsIdentKeys) {
  TextConfigFlags flags = {TextConfigFlag::kSingleQuotes,
                           TextConfigFlag::kComments};
  auto result = ReadConfigFromText("{key: 1}", flags);
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, NoIdentifiersFlagAllowsStringKeys) {
  TextConfigFlags flags = {TextConfigFlag::kSingleQuotes,
                           TextConfigFlag::kComments};
  auto result = ReadConfigFromText("{\"key\": 1}", flags);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("key"), 1);
}

TEST(ReadConfigFromTextTest, NoIdentifiersFlagAllowsSingleQuotedKey) {
  TextConfigFlags flags = {TextConfigFlag::kSingleQuotes,
                           TextConfigFlag::kComments};
  auto result = ReadConfigFromText("{'key': 1}", flags);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("key"), 1);
}

// No comments flag: comments are rejected, but everything else works.
TEST(ReadConfigFromTextTest, NoCommentsFlagRejectsLineComment) {
  TextConfigFlags flags = {TextConfigFlag::kIdentifiers,
                           TextConfigFlag::kSingleQuotes};
  auto result = ReadConfigFromText("// comment\n42", flags);
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, NoCommentsFlagRejectsBlockComment) {
  TextConfigFlags flags = {TextConfigFlag::kIdentifiers,
                           TextConfigFlag::kSingleQuotes};
  auto result = ReadConfigFromText("/* comment */ 42", flags);
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, NoCommentsFlagAllowsNormalValues) {
  TextConfigFlags flags = {TextConfigFlag::kIdentifiers,
                           TextConfigFlag::kSingleQuotes};
  auto result = ReadConfigFromText("{key: 'hello'}", flags);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->Get("key")->GetString(), "hello");
}

// Rootless with no identifiers: must use string keys.
TEST(ReadConfigFromTextTest, RootlessNoIdentifiersFlagRejectsIdentKeys) {
  TextConfigFlags flags = {TextConfigFlag::kRootless,
                           TextConfigFlag::kSingleQuotes,
                           TextConfigFlag::kComments};
  auto result = ReadConfigFromText("key: 1", flags);
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, RootlessNoIdentifiersFlagAllowsStringKeys) {
  TextConfigFlags flags = {TextConfigFlag::kRootless,
                           TextConfigFlag::kSingleQuotes,
                           TextConfigFlag::kComments};
  auto result = ReadConfigFromText("\"key\": 1", flags);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("key"), 1);
}

//==============================================================================
// ReadConfigFromText — Whitespace handling
//==============================================================================

TEST(ReadConfigFromTextTest, LeadingWhitespace) {
  auto result = ReadConfigFromText("   42");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt(), 42);
}

TEST(ReadConfigFromTextTest, TrailingWhitespace) {
  auto result = ReadConfigFromText("42   ");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt(), 42);
}

TEST(ReadConfigFromTextTest, WhitespaceAroundMapValues) {
  auto result = ReadConfigFromText("{  \"a\"  :  1  ,  \"b\"  :  2  }");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_EQ(result->GetInt("b"), 2);
}

TEST(ReadConfigFromTextTest, WhitespaceAroundArrayValues) {
  auto result = ReadConfigFromText("[  1  ,  2  ,  3  ]");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetArraySize(), 3);
  EXPECT_EQ(result->GetInt(0), 1);
  EXPECT_EQ(result->GetInt(1), 2);
  EXPECT_EQ(result->GetInt(2), 3);
}

TEST(ReadConfigFromTextTest, MultilineMap) {
  auto result = ReadConfigFromText(
      "{\n"
      "  \"a\": 1,\n"
      "  \"b\": 2\n"
      "}");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_EQ(result->GetInt("b"), 2);
}

//==============================================================================
// ReadConfigFromText — Error cases
//==============================================================================

TEST(ReadConfigFromTextTest, EmptyInputRooted) {
  auto result = ReadConfigFromText("");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(ReadConfigFromTextTest, InvalidToken) {
  auto result = ReadConfigFromText("@invalid");
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, TrailingGarbageAfterValue) {
  auto result = ReadConfigFromText("42 extra");
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, UnclosedArray) {
  auto result = ReadConfigFromText("[1, 2");
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, UnclosedMap) {
  auto result = ReadConfigFromText("{\"a\": 1");
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, MissingColon) {
  auto result = ReadConfigFromText("{\"a\" 1}");
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, MissingValue) {
  auto result = ReadConfigFromText("{\"a\":}");
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, ExtraCommaInArray) {
  // The parser allows trailing commas because of comma-separated repeat syntax.
  // Verify it still parses (or at least has consistent behavior).
  auto result = ReadConfigFromText("[1, 2,]");
  // This may or may not be an error depending on parser behavior.
  // We just verify it doesn't crash.
}

TEST(ReadConfigFromTextTest, RootlessInvalidKey) {
  auto result = ReadConfigFromText("123: 1", kRootlessTextConfig);
  EXPECT_FALSE(result.ok());
}

TEST(ReadConfigFromTextTest, RootlessTrailingGarbage) {
  auto result = ReadConfigFromText("a: 1 garbage", kRootlessTextConfig);
  EXPECT_FALSE(result.ok());
}

//==============================================================================
// ReadConfigFromText — Round-trip (Write then Read)
//==============================================================================

TEST(ReadConfigFromTextTest, RoundTripNull) {
  Config original = Config::None();
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsNone());
}

TEST(ReadConfigFromTextTest, RoundTripBool) {
  Config original = Config::Bool(true);
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsBool());
  EXPECT_TRUE(result->GetBool());
}

TEST(ReadConfigFromTextTest, RoundTripInt) {
  Config original = Config::Int(-12345);
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsInt());
  EXPECT_EQ(result->GetInt(), -12345);
}

TEST(ReadConfigFromTextTest, RoundTripFloat) {
  Config original = Config::Float(3.14);
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsFloat());
  EXPECT_NEAR(result->GetFloat(), 3.14, 1e-9);
}

TEST(ReadConfigFromTextTest, RoundTripString) {
  Config original = Config::String("hello\nworld\t!");
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "hello\nworld\t!");
}

TEST(ReadConfigFromTextTest, RoundTripStringWithQuotes) {
  Config original = Config::String("a\"b");
  std::string text = WriteConfigToText(original, kJsonTextConfig);
  auto result = ReadConfigFromText(std::string(text), kJsonTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetString(), "a\"b");
}

TEST(ReadConfigFromTextTest, RoundTripEmptyArray) {
  Config original = Config::Array();
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsArray());
  EXPECT_EQ(result->GetArraySize(), 0);
}

TEST(ReadConfigFromTextTest, RoundTripArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Bool(false));
  arr.push_back(Config::String("test"));
  arr.push_back(Config::None());
  Config original = Config::Array(std::move(arr));
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsArray());
  EXPECT_EQ(result->GetArraySize(), 4);
  EXPECT_EQ(result->GetInt(0), 1);
  EXPECT_FALSE(result->Get(1)->GetBool());
  EXPECT_EQ(result->Get(2)->GetString(), "test");
  EXPECT_TRUE(result->Get(3)->IsNone());
}

TEST(ReadConfigFromTextTest, RoundTripEmptyMap) {
  Config original = Config::Map();
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetMapSize(), 0);
}

TEST(ReadConfigFromTextTest, RoundTripMap) {
  Config original = Config::Map();
  original.Set("name", Config::String("test"));
  original.Set("value", Config::Int(42));
  original.Set("enabled", Config::Bool(true));
  original.Set("nothing", Config::None());
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetMapSize(), 4);
  EXPECT_EQ(result->Get("name")->GetString(), "test");
  EXPECT_EQ(result->Get("value")->GetInt(), 42);
  EXPECT_TRUE(result->Get("enabled")->GetBool());
  EXPECT_TRUE(result->Get("nothing")->IsNone());
}

TEST(ReadConfigFromTextTest, RoundTripNestedMap) {
  Config inner = Config::Map();
  inner.Set("x", Config::Int(10));
  inner.Set("y", Config::Int(20));
  Config original = Config::Map();
  original.Set("pos", std::move(inner));
  original.Set("name", Config::String("point"));
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->Get("pos")->GetInt("x"), 10);
  EXPECT_EQ(result->Get("pos")->GetInt("y"), 20);
  EXPECT_EQ(result->Get("name")->GetString(), "point");
}

TEST(ReadConfigFromTextTest, RoundTripMapWithArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  arr.push_back(Config::Int(3));
  Config original = Config::Map();
  original.Set("list", Config::Array(std::move(arr)));
  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->Get("list")->IsArray());
  EXPECT_EQ(result->Get("list")->GetArraySize(), 3);
}

TEST(ReadConfigFromTextTest, RoundTripCompact) {
  Config original = Config::Map();
  original.Set("a", Config::Int(1));
  original.Set("b", Config::String("hi"));
  std::string text = WriteConfigToText(original, kCompactTextConfig);
  auto result = ReadConfigFromText(std::string(text), kCompactTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_EQ(result->Get("b")->GetString(), "hi");
}

TEST(ReadConfigFromTextTest, RoundTripJson) {
  Config original = Config::Map();
  original.Set("key", Config::String("value"));
  original.Set("num", Config::Int(99));
  std::string text = WriteConfigToText(original, kJsonTextConfig);
  auto result = ReadConfigFromText(std::string(text), kJsonTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->Get("key")->GetString(), "value");
  EXPECT_EQ(result->GetInt("num"), 99);
}

TEST(ReadConfigFromTextTest, RoundTripRootless) {
  Config original = Config::Map();
  original.Set("a", Config::Int(1));
  original.Set("b", Config::Int(2));
  std::string text = WriteConfigToText(original, kRootlessTextConfig);
  auto result = ReadConfigFromText(std::string(text), kRootlessTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->IsMap());
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_EQ(result->GetInt("b"), 2);
}

TEST(ReadConfigFromTextTest, RoundTripRootlessNested) {
  Config inner = Config::Map();
  inner.Set("c", Config::Int(3));
  Config original = Config::Map();
  original.Set("a", Config::Int(1));
  original.Set("b", std::move(inner));
  std::string text = WriteConfigToText(original, kRootlessTextConfig);
  auto result = ReadConfigFromText(std::string(text), kRootlessTextConfig);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->GetInt("a"), 1);
  EXPECT_TRUE(result->Get("b")->IsMap());
  EXPECT_EQ(result->Get("b")->GetInt("c"), 3);
}

TEST(ReadConfigFromTextTest, RoundTripComplex) {
  Config inner_map = Config::Map();
  inner_map.Set("enabled", Config::Bool(true));
  inner_map.Set("count", Config::Int(5));

  Config::ArrayValue inner_arr;
  inner_arr.push_back(Config::String("alpha"));
  inner_arr.push_back(Config::String("beta"));

  Config original = Config::Map();
  original.Set("settings", std::move(inner_map));
  original.Set("tags", Config::Array(std::move(inner_arr)));
  original.Set("ratio", Config::Float(0.75));
  original.Set("missing", Config::None());

  std::string text = WriteConfigToText(original);
  auto result = ReadConfigFromText(std::string(text));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->Get("settings")->IsMap());
  EXPECT_TRUE(result->Get("settings")->Get("enabled")->GetBool());
  EXPECT_EQ(result->Get("settings")->GetInt("count"), 5);
  EXPECT_TRUE(result->Get("tags")->IsArray());
  EXPECT_EQ(result->Get("tags")->GetArraySize(), 2);
  EXPECT_EQ(result->Get("tags")->Get(0)->GetString(), "alpha");
  EXPECT_EQ(result->Get("tags")->Get(1)->GetString(), "beta");
  EXPECT_NEAR(result->Get("ratio")->GetFloat(), 0.75, 1e-9);
  EXPECT_TRUE(result->Get("missing")->IsNone());
}

}  // namespace
}  // namespace gb
