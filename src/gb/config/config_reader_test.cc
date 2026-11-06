// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/config/config_reader.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gb/config/config.h"
#include "gb/config/text_config.h"
#include "gb/test/thread_tester.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

enum class Color { kNone, kRed, kGreen, kBlue };

constexpr ConfigEnumName<Color> kColors[] = {
    {"red", Color::kRed},
    {"green", Color::kGreen},
    {"blue", Color::kBlue},
    {"crimson", Color::kRed},
};

template <typename T>
concept CanReadInt =
    requires(const ConfigReader& reader, T* value) { reader.ReadInt(value); };

template <typename T>
concept CanReadFloat =
    requires(const ConfigReader& reader, T* value) { reader.ReadFloat(value); };

static_assert(CanReadInt<int>);
static_assert(CanReadInt<int8_t>);
static_assert(CanReadInt<uint64_t>);
static_assert(!CanReadInt<bool>);
static_assert(!CanReadInt<char>);
static_assert(!CanReadInt<char8_t>);
static_assert(!CanReadInt<wchar_t>);
static_assert(!CanReadInt<float>);
static_assert(!CanReadInt<Color>);
static_assert(CanReadFloat<float>);
static_assert(CanReadFloat<double>);
static_assert(!CanReadFloat<int>);
static_assert(!CanReadFloat<int64_t>);

// Parses `text`, which must be a valid text config.
Config Parse(std::string text) {
  absl::StatusOr<Config> config = ReadConfigFromText(std::move(text));
  EXPECT_TRUE(config.ok()) << config.status();
  return config.ok() ? *std::move(config) : Config();
}

// Returns a map from `key` to `value`, made in code so it has no locations.
Config MapOf(std::string_view key, Config value) {
  Config config;
  config.Set(key, std::move(value));
  return config;
}

// Returns "ok", or the reader's error message, which must be InvalidArgument.
std::string Error(const ConfigReader& reader) {
  const absl::Status status = reader.GetStatus();
  if (status.ok()) {
    return "ok";
  }
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  return std::string(status.message());
}

//==============================================================================
// Values and paths
//==============================================================================

TEST(ConfigReaderTest, Root) {
  const Config config = Config::Int(5);
  ConfigReader reader(&config);
  EXPECT_EQ(reader.GetConfig(), &config);
  EXPECT_FALSE(reader.IsMissing());
  EXPECT_EQ(reader.GetPath(), "");
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, NullRootIsMissing) {
  ConfigReader reader(nullptr);
  EXPECT_TRUE(reader.IsMissing());
  EXPECT_EQ(reader.GetConfig(), nullptr);
  int value = 3;
  reader.ReadInt(&value);
  EXPECT_EQ(value, 3);
  EXPECT_FALSE(reader.ValidateMap({}));
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, GetKey) {
  const Config config = Parse("{a: 1, b: 2}");
  ConfigReader reader(&config);
  ConfigReader a = reader.Get("a");
  EXPECT_EQ(a.GetConfig(), config.Get("a"));
  EXPECT_FALSE(a.IsMissing());
  EXPECT_EQ(a.GetPath(), "a");
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, GetMissingKey) {
  const Config config = Parse("{a: 1}");
  ConfigReader reader(&config);
  ConfigReader b = reader.Get("b");
  EXPECT_TRUE(b.IsMissing());
  EXPECT_EQ(b.GetConfig(), nullptr);
  EXPECT_EQ(b.GetPath(), "b");
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, GetIndex) {
  const Config config = Parse("[1, 2, 3]");
  ConfigReader reader(&config);
  EXPECT_EQ(reader.GetArraySize(), 3);
  ConfigReader second = reader.Get(1);
  EXPECT_EQ(second.GetConfig(), config.Get(1));
  EXPECT_EQ(second.GetPath(), "[1]");
  EXPECT_TRUE(reader.Get(3).IsMissing());
  EXPECT_TRUE(reader.Get(-1).IsMissing());
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, GetArraySizeOfNonArray) {
  const Config config = Parse("{a: 1}");
  ConfigReader reader(&config);
  EXPECT_EQ(reader.GetArraySize(), 0);
  EXPECT_EQ(reader.Get("b").GetArraySize(), 0);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, Paths) {
  const Config config = Parse("{a: [{b: {c: 1}}, 2]}");
  ConfigReader reader(&config);
  EXPECT_EQ(reader.Get("a").Get(0).Get("b").Get("c").GetPath(), "a[0].b.c");
  EXPECT_EQ(reader.Get("a").Get(1).GetPath(), "a[1]");
  EXPECT_EQ(reader.Get("x").Get(2).Get("y").GetPath(), "x[2].y");
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, GetKeyOfNonMap) {
  const Config config = MapOf("a", Config::Int(1));
  ConfigReader reader(&config);
  ConfigReader b = reader.Get("a").Get("b");
  EXPECT_TRUE(b.IsMissing());
  EXPECT_EQ(b.GetPath(), "a.b");
  EXPECT_EQ(Error(reader), "a: must be a map");
}

TEST(ConfigReaderTest, GetIndexOfNonArray) {
  const Config config = MapOf("a", Config::Int(1));
  ConfigReader reader(&config);
  ConfigReader a0 = reader.Get("a").Get(0);
  EXPECT_TRUE(a0.IsMissing());
  EXPECT_EQ(a0.GetPath(), "a[0]");
  EXPECT_EQ(Error(reader), "a: must be an array");
}

TEST(ConfigReaderTest, GetFromMissingValueIsMissing) {
  const Config config = Config::Map();
  ConfigReader reader(&config);
  EXPECT_TRUE(reader.Get("a").Get("b").IsMissing());
  EXPECT_TRUE(reader.Get("a").Get(0).IsMissing());
  EXPECT_EQ(Error(reader), "ok");
}

//==============================================================================
// ValidateMap and ValidateArray
//==============================================================================

TEST(ConfigReaderTest, ValidateMapWithKnownKeys) {
  const Config config = Parse("{a: 1, b: 2}");
  ConfigReader reader(&config);
  EXPECT_TRUE(reader.ValidateMap({"a", "b", "c"}));
  EXPECT_TRUE(reader.ValidateMap({"b", "a"}));
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ValidateEmptyMap) {
  const Config config = Config::Map();
  ConfigReader reader(&config);
  EXPECT_TRUE(reader.ValidateMap({}));
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ValidateMapNamesFirstUnknownKeyInOrder) {
  Config config;
  config.SetInt("x", 1);
  config.SetInt("zeta", 2);
  config.SetInt("beta", 3);
  config.SetInt("alpha", 4);
  ConfigReader reader(&config);
  EXPECT_FALSE(reader.ValidateMap({"x", "alpha"}));
  EXPECT_EQ(Error(reader), "beta: unknown key");
}

TEST(ConfigReaderTest, ValidateMapUnknownKeyIsAtTheKey) {
  const Config config = Parse("{\n  a: {\n    b: 1,\n    c: 2\n  }\n}");
  ConfigReader reader(&config, "hello.cfg");
  EXPECT_TRUE(reader.ValidateMap({"a"}));
  EXPECT_FALSE(reader.Get("a").ValidateMap({"b"}));
  EXPECT_EQ(Error(reader), "hello.cfg(4:8): a.c: unknown key");
}

TEST(ConfigReaderTest, ValidateMapOfNonMap) {
  const Config config = MapOf("a", Config::Array());
  ConfigReader reader(&config);
  EXPECT_FALSE(reader.Get("a").ValidateMap({}));
  EXPECT_EQ(Error(reader), "a: must be a map");
}

TEST(ConfigReaderTest, ValidateMissingMap) {
  const Config config = Config::Map();
  ConfigReader reader(&config);
  EXPECT_FALSE(reader.Get("a").ValidateMap({"b"}));
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ValidateArraySizes) {
  const Config config = Parse("[1, 2, 3]");
  ConfigReader reader(&config);
  EXPECT_TRUE(reader.ValidateArray());
  EXPECT_TRUE(reader.ValidateArray(3));
  EXPECT_TRUE(reader.ValidateArray(1, 3));
  EXPECT_TRUE(reader.ValidateArray(3, 3));
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ValidateEmptyArray) {
  const Config config = Config::Array();
  ConfigReader reader(&config);
  EXPECT_TRUE(reader.ValidateArray());
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ValidateArrayTooBig) {
  const Config config =
      MapOf("a", Config::Array(Config::ArrayValue(3, Config::Int(1))));
  ConfigReader reader(&config);
  EXPECT_FALSE(reader.Get("a").ValidateArray(1, 2));
  EXPECT_EQ(Error(reader), "a: must have 1 to 2 values");
}

TEST(ConfigReaderTest, ValidateArrayTooSmall) {
  const Config config =
      MapOf("a", Config::Array(Config::ArrayValue(3, Config::Int(1))));
  ConfigReader reader(&config);
  EXPECT_FALSE(reader.Get("a").ValidateArray(4, 8));
  EXPECT_EQ(Error(reader), "a: must have 4 to 8 values");
}

TEST(ConfigReaderTest, ValidateArrayAtLeast) {
  const Config config = MapOf("a", Config::Array());
  ConfigReader reader(&config);
  EXPECT_FALSE(reader.Get("a").ValidateArray(1));
  EXPECT_EQ(Error(reader), "a: must have at least 1 value");
}

TEST(ConfigReaderTest, ValidateArrayExactSize) {
  const Config config = MapOf("a", Config::Array());
  ConfigReader reader(&config);
  EXPECT_FALSE(reader.Get("a").ValidateArray(2, 2));
  EXPECT_EQ(Error(reader), "a: must have 2 values");
}

TEST(ConfigReaderTest, ValidateArrayOfNonArray) {
  const Config config = MapOf("a", Config::Map());
  ConfigReader reader(&config);
  EXPECT_FALSE(reader.Get("a").ValidateArray());
  EXPECT_EQ(Error(reader), "a: must be an array");
}

TEST(ConfigReaderTest, ValidateMissingArray) {
  const Config config = Config::Map();
  ConfigReader reader(&config);
  EXPECT_FALSE(reader.Get("a").ValidateArray(1, 2));
  EXPECT_EQ(Error(reader), "ok");
}

//==============================================================================
// ReadBool and ReadString
//==============================================================================

TEST(ConfigReaderTest, ReadBool) {
  const Config config = Parse("{a: true, b: false}");
  ConfigReader reader(&config);
  bool a = false;
  bool b = true;
  reader.Get("a").ReadBool(&a);
  reader.Get("b").ReadBool(&b);
  EXPECT_TRUE(a);
  EXPECT_FALSE(b);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadMissingBoolKeepsDefault) {
  const Config config = Config::Map();
  ConfigReader reader(&config);
  bool value = true;
  reader.Get("a").ReadBool(&value);
  EXPECT_TRUE(value);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadBoolOfWrongType) {
  const Config config = MapOf("a", Config::Int(1));
  ConfigReader reader(&config);
  bool value = false;
  reader.Get("a").ReadBool(&value);
  EXPECT_FALSE(value);
  EXPECT_EQ(Error(reader), "a: must be a bool");
}

TEST(ConfigReaderTest, ReadString) {
  const Config config = Parse("{a: 'hello'}");
  ConfigReader reader(&config);
  std::string value;
  reader.Get("a").ReadString(&value);
  EXPECT_EQ(value, "hello");
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadMissingStringKeepsDefault) {
  const Config config = Config::Map();
  ConfigReader reader(&config);
  std::string value = "default";
  reader.Get("a").ReadString(&value);
  EXPECT_EQ(value, "default");
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadStringOfWrongType) {
  const Config config = MapOf("a", Config::Int(1));
  ConfigReader reader(&config);
  std::string value = "default";
  reader.Get("a").ReadString(&value);
  EXPECT_EQ(value, "default");
  EXPECT_EQ(Error(reader), "a: must be a string");
}

//==============================================================================
// ReadInt
//==============================================================================

TEST(ConfigReaderTest, ReadInt) {
  const Config config = Parse("{a: 42, b: -7}");
  ConfigReader reader(&config);
  int a = 0;
  int64_t b = 0;
  reader.Get("a").ReadInt(&a);
  reader.Get("b").ReadInt(&b);
  EXPECT_EQ(a, 42);
  EXPECT_EQ(b, -7);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadMissingIntKeepsDefault) {
  const Config config = Config::Map();
  ConfigReader reader(&config);
  int value = 5;
  reader.Get("a").ReadInt(&value);
  reader.Get("a").ReadInt(1, 2, &value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadIntInRange) {
  const Config config = Parse("{a: 1, b: 132}");
  ConfigReader reader(&config);
  int a = 0;
  int b = 0;
  reader.Get("a").ReadInt(1, 132, &a);
  reader.Get("b").ReadInt(1, 132, &b);
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, 132);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadIntAboveRange) {
  const Config config = MapOf("a", Config::Int(133));
  ConfigReader reader(&config);
  int value = 5;
  reader.Get("a").ReadInt(1, 132, &value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be 1 to 132");
}

TEST(ConfigReaderTest, ReadIntBelowRange) {
  const Config config = MapOf("a", Config::Int(0));
  ConfigReader reader(&config);
  int value = 5;
  reader.Get("a").ReadInt(1, 132, &value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be 1 to 132");
}

TEST(ConfigReaderTest, ReadIntOfFloat) {
  const Config config = MapOf("a", Config::Float(2.0));
  ConfigReader reader(&config);
  int value = 5;
  reader.Get("a").ReadInt(&value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be an int");
}

TEST(ConfigReaderTest, ReadIntOfWrongType) {
  const Config config = MapOf("a", Config::String("2"));
  ConfigReader reader(&config);
  int value = 5;
  reader.Get("a").ReadInt(&value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be an int");
}

TEST(ConfigReaderTest, ReadInt8Limits) {
  const Config config = Parse("[-128, 127]");
  ConfigReader reader(&config);
  int8_t low = 0;
  int8_t high = 0;
  reader.Get(0).ReadInt(&low);
  reader.Get(1).ReadInt(&high);
  EXPECT_EQ(low, -128);
  EXPECT_EQ(high, 127);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadInt8AboveLimit) {
  const Config config = MapOf("a", Config::Int(128));
  ConfigReader reader(&config);
  int8_t value = 5;
  reader.Get("a").ReadInt(&value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be -128 to 127");
}

TEST(ConfigReaderTest, ReadInt8BelowLimit) {
  const Config config = MapOf("a", Config::Int(-129));
  ConfigReader reader(&config);
  int8_t value = 5;
  reader.Get("a").ReadInt(&value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be -128 to 127");
}

TEST(ConfigReaderTest, ReadUint8Limits) {
  const Config config = Parse("[0, 255]");
  ConfigReader reader(&config);
  uint8_t low = 5;
  uint8_t high = 0;
  reader.Get(0).ReadInt(&low);
  reader.Get(1).ReadInt(&high);
  EXPECT_EQ(low, 0);
  EXPECT_EQ(high, 255);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadUint8OfNegative) {
  const Config config = MapOf("a", Config::Int(-1));
  ConfigReader reader(&config);
  uint8_t value = 5;
  reader.Get("a").ReadInt(&value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be 0 to 255");
}

TEST(ConfigReaderTest, ReadUint8InRange) {
  const Config config = MapOf("a", Config::Int(9));
  ConfigReader reader(&config);
  uint8_t value = 5;
  reader.Get("a").ReadInt(1, 8, &value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be 1 to 8");
}

TEST(ConfigReaderTest, ReadUint16Limit) {
  const Config config = Parse("[65535, 65536]");
  ConfigReader reader(&config);
  uint16_t value = 0;
  reader.Get(0).ReadInt(&value);
  EXPECT_EQ(value, 65535);
  reader.Get(1).ReadInt(&value);
  EXPECT_EQ(value, 65535);
  EXPECT_EQ(Error(reader), "(1:9): [1]: must be 0 to 65535");
}

TEST(ConfigReaderTest, ReadInt64Limits) {
  Config config;
  config.AppendInt(std::numeric_limits<int64_t>::min());
  config.AppendInt(std::numeric_limits<int64_t>::max());
  ConfigReader reader(&config);
  int64_t low = 0;
  int64_t high = 0;
  reader.Get(0).ReadInt(&low);
  reader.Get(1).ReadInt(&high);
  EXPECT_EQ(low, std::numeric_limits<int64_t>::min());
  EXPECT_EQ(high, std::numeric_limits<int64_t>::max());
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadUint64Limits) {
  Config config;
  config.AppendInt(0);
  config.AppendInt(std::numeric_limits<int64_t>::max());
  ConfigReader reader(&config);
  uint64_t low = 5;
  uint64_t high = 0;
  reader.Get(0).ReadInt(&low);
  reader.Get(1).ReadInt(&high);
  EXPECT_EQ(low, uint64_t{0});
  EXPECT_EQ(high, static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadUint64OfNegative) {
  const Config config = MapOf("a", Config::Int(-1));
  ConfigReader reader(&config);
  uint64_t value = 5;
  reader.Get("a").ReadInt(&value);
  EXPECT_EQ(value, uint64_t{5});
  EXPECT_EQ(Error(reader), "a: must be 0 to 9223372036854775807");
}

//==============================================================================
// ReadFloat
//==============================================================================

TEST(ConfigReaderTest, ReadFloat) {
  const Config config = Parse("{a: 1.5, b: -2.25}");
  ConfigReader reader(&config);
  double a = 0;
  float b = 0;
  reader.Get("a").ReadFloat(&a);
  reader.Get("b").ReadFloat(&b);
  EXPECT_EQ(a, 1.5);
  EXPECT_EQ(b, -2.25f);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadFloatOfInt) {
  const Config config = Parse("{a: 60, b: -3}");
  ConfigReader reader(&config);
  double a = 0;
  float b = 0;
  reader.Get("a").ReadFloat(&a);
  reader.Get("b").ReadFloat(&b);
  EXPECT_EQ(a, 60.0);
  EXPECT_EQ(b, -3.0f);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadMissingFloatKeepsDefault) {
  const Config config = Config::Map();
  ConfigReader reader(&config);
  double value = 5;
  reader.Get("a").ReadFloat(&value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadFloatOfWrongType) {
  const Config config = MapOf("a", Config::Bool(true));
  ConfigReader reader(&config);
  double value = 5;
  reader.Get("a").ReadFloat(&value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be a float");
}

TEST(ConfigReaderTest, ReadBigFloat) {
  const Config config = Parse("[1e38, -1e38, 1e300]");
  ConfigReader reader(&config);
  float low = 0;
  float high = 0;
  double big = 0;
  reader.Get(0).ReadFloat(&high);
  reader.Get(1).ReadFloat(&low);
  reader.Get(2).ReadFloat(&big);
  EXPECT_EQ(high, 1e38f);
  EXPECT_EQ(low, -1e38f);
  EXPECT_EQ(big, 1e300);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadFloatTooBigForFloat) {
  const Config config = MapOf("a", Config::Float(1e39));
  ConfigReader reader(&config);
  float value = 5;
  reader.Get("a").ReadFloat(&value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be -3.40282e+38 to 3.40282e+38");
}

TEST(ConfigReaderTest, ReadNegativeFloatTooBigForFloat) {
  const Config config = MapOf("a", Config::Float(-1e39));
  ConfigReader reader(&config);
  float value = 5;
  reader.Get("a").ReadFloat(&value);
  EXPECT_EQ(value, 5);
  EXPECT_EQ(Error(reader), "a: must be -3.40282e+38 to 3.40282e+38");
}

TEST(ConfigReaderTest, ReadInfiniteFloat) {
  const Config config = Config::Float(std::numeric_limits<double>::infinity());
  ConfigReader reader(&config);
  float value = 5;
  reader.ReadFloat(&value);
  EXPECT_EQ(value, std::numeric_limits<float>::infinity());
  EXPECT_EQ(Error(reader), "ok");
}

//==============================================================================
// ReadEnum
//==============================================================================

TEST(ConfigReaderTest, ReadEnum) {
  const Config config = Parse("['red', 'green', 'blue', 'crimson']");
  ConfigReader reader(&config);
  std::vector<Color> colors(4, Color::kNone);
  for (int i = 0; i < 4; ++i) {
    reader.Get(i).ReadEnum<Color>(kColors, &colors[i]);
  }
  EXPECT_EQ(colors[0], Color::kRed);
  EXPECT_EQ(colors[1], Color::kGreen);
  EXPECT_EQ(colors[2], Color::kBlue);
  EXPECT_EQ(colors[3], Color::kRed);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadMissingEnumKeepsDefault) {
  const Config config = Config::Map();
  ConfigReader reader(&config);
  Color value = Color::kBlue;
  reader.Get("a").ReadEnum<Color>(kColors, &value);
  EXPECT_EQ(value, Color::kBlue);
  EXPECT_EQ(Error(reader), "ok");
}

TEST(ConfigReaderTest, ReadEnumOfUnknownName) {
  const Config config = MapOf("a", Config::String("Red"));
  ConfigReader reader(&config);
  Color value = Color::kNone;
  reader.Get("a").ReadEnum<Color>(kColors, &value);
  EXPECT_EQ(value, Color::kNone);
  EXPECT_EQ(Error(reader),
            "a: must be \"red\", \"green\", \"blue\", or \"crimson\"");
}

TEST(ConfigReaderTest, ReadEnumOfNonString) {
  const Config config = MapOf("a", Config::Int(1));
  ConfigReader reader(&config);
  Color value = Color::kNone;
  reader.Get("a").ReadEnum<Color>(kColors, &value);
  EXPECT_EQ(value, Color::kNone);
  EXPECT_EQ(Error(reader),
            "a: must be \"red\", \"green\", \"blue\", or \"crimson\"");
}

TEST(ConfigReaderTest, ReadEnumOfTwoNames) {
  constexpr ConfigEnumName<Color> kNames[] = {{"red", Color::kRed},
                                              {"blue", Color::kBlue}};
  const Config config = MapOf("a", Config::String("green"));
  ConfigReader reader(&config);
  Color value = Color::kNone;
  reader.Get("a").ReadEnum<Color>(kNames, &value);
  EXPECT_EQ(Error(reader), "a: must be \"red\" or \"blue\"");
}

TEST(ConfigReaderTest, ReadEnumOfOneName) {
  constexpr ConfigEnumName<Color> kNames[] = {{"red", Color::kRed}};
  const Config config = MapOf("a", Config::String("green"));
  ConfigReader reader(&config);
  Color value = Color::kNone;
  reader.Get("a").ReadEnum<Color>(kNames, &value);
  EXPECT_EQ(Error(reader), "a: must be \"red\"");
}

//==============================================================================
// Errors
//==============================================================================

TEST(ConfigReaderTest, RejectAtRoot) {
  const Config config = Config::Map();
  ConfigReader reader(&config);
  reader.Reject("bad");
  EXPECT_EQ(Error(reader), "bad");
}

TEST(ConfigReaderTest, RejectAtRootWithSource) {
  const Config config = Config::Map();
  ConfigReader reader(&config, "hello.cfg");
  reader.Reject("bad");
  EXPECT_EQ(Error(reader), "hello.cfg: bad");
}

TEST(ConfigReaderTest, RejectAtLocatedRoot) {
  const Config config = Parse("\n  {a: 1}");
  ConfigReader reader(&config, "hello.cfg");
  reader.Reject("bad");
  EXPECT_EQ(Error(reader), "hello.cfg(2:3): bad");
}

TEST(ConfigReaderTest, RejectWithSourceAndLocation) {
  const Config config = Parse("{\n  a: 1\n}");
  ConfigReader reader(&config, "hello.cfg");
  reader.Get("a").Reject("bad");
  EXPECT_EQ(Error(reader), "hello.cfg(2:6): a: bad");
}

TEST(ConfigReaderTest, RejectWithLocationAndNoSource) {
  const Config config = Parse("{\n  a: 1\n}");
  ConfigReader reader(&config);
  reader.Get("a").Reject("bad");
  EXPECT_EQ(Error(reader), "(2:6): a: bad");
}

TEST(ConfigReaderTest, RejectWithSourceAndNoLocation) {
  const Config config = MapOf("a", Config::Int(1));
  ConfigReader reader(&config, "hello.cfg");
  reader.Get("a").Reject("bad");
  EXPECT_EQ(Error(reader), "hello.cfg: a: bad");
}

TEST(ConfigReaderTest, RejectWithNoSourceOrLocation) {
  const Config config = MapOf("a", Config::Int(1));
  ConfigReader reader(&config);
  reader.Get("a").Reject("bad");
  EXPECT_EQ(Error(reader), "a: bad");
}

TEST(ConfigReaderTest, RejectMissingValueGivesItsMapLocation) {
  const Config config = Parse("{\n  device: {\n    columns: 80\n  }\n}");
  ConfigReader reader(&config, "hello.cfg");
  ConfigReader type = reader.Get("device").Get("type");
  ASSERT_TRUE(type.IsMissing());
  type.Reject("missing type");
  EXPECT_EQ(Error(reader), "hello.cfg(2:11): device.type: missing type");
}

TEST(ConfigReaderTest, RejectValueMissingFromMissingValue) {
  const Config config = Parse("{\n  a: {}\n}");
  ConfigReader reader(&config, "hello.cfg");
  reader.Get("a").Get("b").Get(1).Get("c").Reject("missing");
  EXPECT_EQ(Error(reader), "hello.cfg(2:6): a.b[1].c: missing");
}

TEST(ConfigReaderTest, OnlyFirstErrorIsKept) {
  const Config config = Parse("{a: 1, b: 2}");
  ConfigReader reader(&config);
  reader.Get("a").Reject("first");
  reader.Get("b").Reject("second");
  reader.Reject("third");
  EXPECT_EQ(Error(reader), "(1:5): a: first");
}

TEST(ConfigReaderTest, ErrorIsSharedByCopiesAndChildren) {
  const Config config = Parse("{a: {b: 1}}");
  ConfigReader reader(&config);
  const ConfigReader a = reader.Get("a");
  const ConfigReader b = a.Get("b");
  const ConfigReader copy = reader;
  b.Reject("bad");
  EXPECT_EQ(Error(reader), "(1:9): a.b: bad");
  EXPECT_EQ(Error(a), "(1:9): a.b: bad");
  EXPECT_EQ(Error(b), "(1:9): a.b: bad");
  EXPECT_EQ(Error(copy), "(1:9): a.b: bad");
}

TEST(ConfigReaderTest, MovedFromReaderKeepsItsRead) {
  const Config config = Parse("{a: 1}");
  ConfigReader reader(&config);
  ConfigReader a = reader.Get("a");
  ConfigReader moved = std::move(a);
  a.Reject("bad");
  EXPECT_EQ(Error(moved), "(1:5): a: bad");
  EXPECT_EQ(Error(reader), "(1:5): a: bad");
}

TEST(ConfigReaderTest, ReadersOfOneReadOnManyThreads) {
  const Config config = Parse("{a: 1, b: 2}");
  const ConfigReader reader(&config);
  ThreadTester tester;
  tester.Run(
      "read",
      [&reader]() {
        for (int i = 0; i < 1000; ++i) {
          int a = 0;
          std::string b;
          reader.Get("a").ReadInt(&a);
          reader.Get("b").ReadString(&b);
          if (a != 1 || reader.GetStatus().ok()) {
            return false;
          }
        }
        return true;
      },
      ThreadTester::MaxConcurrency());
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(Error(reader), "(1:11): b: must be a string");
}

TEST(ConfigReaderTest, SeparateReadsHaveSeparateErrors) {
  const Config config = Parse("{a: 1}");
  ConfigReader first(&config);
  ConfigReader second(&config);
  first.Reject("bad");
  EXPECT_EQ(Error(first), "(1:1): bad");
  EXPECT_EQ(Error(second), "ok");
}

TEST(ConfigReaderTest, ReadsStillReadAfterAnError) {
  const Config config = Parse("{a: 'x', b: 7, c: 'y'}");
  ConfigReader reader(&config);
  int a = 0;
  int b = 0;
  int c = 0;
  reader.Get("a").ReadInt(&a);
  reader.Get("b").ReadInt(&b);
  reader.Get("c").ReadInt(&c);
  EXPECT_EQ(a, 0);
  EXPECT_EQ(b, 7);
  EXPECT_EQ(c, 0);
  EXPECT_EQ(Error(reader), "(1:5): a: must be an int");
}

//==============================================================================
// End to end
//==============================================================================

constexpr std::string_view kMachineKeys[] = {"clock_hz", "devices"};
constexpr std::string_view kDeviceKeys[] = {"type", "columns", "rows"};

enum class DeviceType { kNone, kScreen, kKeyboard };

constexpr ConfigEnumName<DeviceType> kDeviceTypes[] = {
    {"screen", DeviceType::kScreen},
    {"keyboard", DeviceType::kKeyboard},
};

struct Device {
  DeviceType type = DeviceType::kNone;
  int columns = 80;
  int rows = 25;
};

struct Machine {
  int64_t clock_hz = 1000;
  std::vector<Device> devices;
};

absl::Status ReadMachine(const Config& config, std::string_view filename,
                         Machine* machine) {
  ConfigReader reader(&config, filename);
  if (reader.ValidateMap(kMachineKeys)) {
    reader.Get("clock_hz").ReadInt(1, 1000000, &machine->clock_hz);
    const ConfigReader devices = reader.Get("devices");
    if (devices.ValidateArray(1, 4)) {
      machine->devices.resize(devices.GetArraySize());
      for (int i = 0; i < devices.GetArraySize(); ++i) {
        const ConfigReader device = devices.Get(i);
        Device* settings = &machine->devices[i];
        if (device.ValidateMap(kDeviceKeys)) {
          const ConfigReader type = device.Get("type");
          if (type.IsMissing()) {
            type.Reject("missing type");
          }
          type.ReadEnum<DeviceType>(kDeviceTypes, &settings->type);
          device.Get("columns").ReadInt(1, 132, &settings->columns);
          device.Get("rows").ReadInt(1, 60, &settings->rows);
        }
      }
    }
  }
  return reader.GetStatus();
}

TEST(ConfigReaderTest, ReadsSettingsFromText) {
  const Config config = Parse(
      "{\n"
      "  clock_hz: 5000,\n"
      "  devices: [\n"
      "    {type: 'screen', columns: 40},\n"
      "    {type: 'keyboard'}\n"
      "  ]\n"
      "}\n");
  Machine machine;
  EXPECT_TRUE(ReadMachine(config, "hello.cfg", &machine).ok());
  EXPECT_EQ(machine.clock_hz, 5000);
  ASSERT_EQ(static_cast<int>(machine.devices.size()), 2);
  EXPECT_EQ(machine.devices[0].type, DeviceType::kScreen);
  EXPECT_EQ(machine.devices[0].columns, 40);
  EXPECT_EQ(machine.devices[0].rows, 25);
  EXPECT_EQ(machine.devices[1].type, DeviceType::kKeyboard);
  EXPECT_EQ(machine.devices[1].columns, 80);
}

TEST(ConfigReaderTest, ReportsErrorLocationInText) {
  const Config config = Parse(
      "{\n"
      "  clock_hz: 5000,\n"
      "  devices: [\n"
      "    {type: 'screen'},\n"
      "    {type: 'screen', columns: 200}\n"
      "  ]\n"
      "}\n");
  Machine machine;
  const absl::Status status = ReadMachine(config, "hello.cfg", &machine);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status.message(),
            "hello.cfg(5:31): devices[1].columns: must be 1 to 132");
}

TEST(ConfigReaderTest, ReportsMissingValueInText) {
  const Config config = Parse(
      "{\n"
      "  devices: [\n"
      "    {columns: 40}\n"
      "  ]\n"
      "}\n");
  Machine machine;
  const absl::Status status = ReadMachine(config, "hello.cfg", &machine);
  EXPECT_EQ(status.message(), "hello.cfg(3:5): devices[0].type: missing type");
}

}  // namespace
}  // namespace gb
