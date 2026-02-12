// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/config/config.h"

#include <cmath>
#include <limits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

//==============================================================================
// Factory functions and type accessors
//==============================================================================

TEST(ConfigTest, CreateBool) {
  Config config = Config::Bool(true);
  EXPECT_EQ(config.GetType(), Config::Type::kBool);
  EXPECT_TRUE(config.IsBool());
  EXPECT_FALSE(config.IsInt());
  EXPECT_TRUE(config.GetBool());
}

TEST(ConfigTest, CreateBoolFalse) {
  Config config = Config::Bool(false);
  EXPECT_EQ(config.GetType(), Config::Type::kBool);
  EXPECT_FALSE(config.GetBool());
}

TEST(ConfigTest, CreateInt) {
  Config config = Config::Int(42);
  EXPECT_EQ(config.GetType(), Config::Type::kInt);
  EXPECT_TRUE(config.IsInt());
  EXPECT_FALSE(config.IsBool());
  EXPECT_EQ(config.GetInt(), 42);
}

TEST(ConfigTest, CreateIntNegative) {
  Config config = Config::Int(-100);
  EXPECT_EQ(config.GetInt(), -100);
}

TEST(ConfigTest, CreateFloat) {
  Config config = Config::Float(3.14);
  EXPECT_EQ(config.GetType(), Config::Type::kFloat);
  EXPECT_TRUE(config.IsFloat());
  EXPECT_EQ(config.GetFloat(), 3.14);
}

TEST(ConfigTest, CreateString) {
  Config config = Config::String("hello");
  EXPECT_EQ(config.GetType(), Config::Type::kString);
  EXPECT_TRUE(config.IsString());
  EXPECT_EQ(config.GetString(), "hello");
}

TEST(ConfigTest, CreateStringFromStringView) {
  std::string_view sv = "world";
  Config config = Config::String(sv);
  EXPECT_EQ(config.GetString(), "world");
}

TEST(ConfigTest, CreateStringFromStdString) {
  std::string s = "test";
  Config config = Config::String(s);
  EXPECT_EQ(config.GetString(), "test");
}

TEST(ConfigTest, CreateMap) {
  Config::MapValue map;
  map.insert_or_assign("a", Config::Int(1));
  map.insert_or_assign("b", Config::Int(2));
  Config config = Config::Map(std::move(map));
  EXPECT_EQ(config.GetType(), Config::Type::kMap);
  EXPECT_TRUE(config.IsMap());
  EXPECT_EQ(config.GetMapSize(), 2);
}

TEST(ConfigTest, CreateArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  arr.push_back(Config::Int(3));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(config.GetType(), Config::Type::kArray);
  EXPECT_TRUE(config.IsArray());
  EXPECT_EQ(config.GetArraySize(), 3);
}

TEST(ConfigTest, DefaultConstructIsNone) {
  Config config;
  EXPECT_EQ(config.GetType(), Config::Type::kNone);
  EXPECT_TRUE(config.IsNone());
  EXPECT_FALSE(config.IsBool());
  EXPECT_FALSE(config.IsInt());
  EXPECT_FALSE(config.IsFloat());
  EXPECT_FALSE(config.IsString());
  EXPECT_FALSE(config.IsMap());
  EXPECT_FALSE(config.IsArray());
}

TEST(ConfigTest, CreateNone) {
  Config config = Config::None();
  EXPECT_EQ(config.GetType(), Config::Type::kNone);
  EXPECT_TRUE(config.IsNone());
}

TEST(ConfigTest, Clear) {
  Config config = Config::Int(42);
  EXPECT_TRUE(config.IsInt());
  config.Clear();
  EXPECT_TRUE(config.IsNone());
  EXPECT_EQ(config.GetType(), Config::Type::kNone);
}

TEST(ConfigTest, CopyConstruct) {
  Config original = Config::Int(42);
  Config copy(original);
  EXPECT_EQ(copy.GetInt(), 42);
  EXPECT_EQ(original.GetInt(), 42);
}

TEST(ConfigTest, MoveConstruct) {
  Config original = Config::String("hello");
  Config moved(std::move(original));
  EXPECT_EQ(moved.GetString(), "hello");
}

TEST(ConfigTest, CopyAssign) {
  Config config = Config::Bool(true);
  Config other = Config::Int(42);
  config = other;
  EXPECT_EQ(config.GetInt(), 42);
}

TEST(ConfigTest, MoveAssign) {
  Config config = Config::Bool(true);
  config = Config::Float(1.5);
  EXPECT_EQ(config.GetFloat(), 1.5);
}

//==============================================================================
// Value accessors — wrong type returns default
//==============================================================================

TEST(ConfigTest, GetBoolWrongType) {
  EXPECT_FALSE(Config::Int(42).GetBool());
  EXPECT_FALSE(Config::String("true").GetBool());
}

TEST(ConfigTest, GetIntWrongType) {
  EXPECT_EQ(Config::Bool(true).GetInt(), 0);
  EXPECT_EQ(Config::String("42").GetInt(), 0);
}

TEST(ConfigTest, GetFloatWrongType) {
  EXPECT_EQ(Config::Int(42).GetFloat(), 0.0);
}

TEST(ConfigTest, GetStringWrongType) {
  EXPECT_EQ(Config::Int(42).GetString(), "");
}

TEST(ConfigTest, GetMapWrongType) {
  EXPECT_EQ(Config::Int(42).GetMap(), nullptr);
}

TEST(ConfigTest, GetArrayWrongType) {
  EXPECT_TRUE(Config::Int(42).GetArray().empty());
}

TEST(ConfigTest, NoneReturnsDefaults) {
  Config config;
  EXPECT_FALSE(config.GetBool());
  EXPECT_EQ(config.GetInt(), 0);
  EXPECT_EQ(config.GetFloat(), 0.0);
  EXPECT_EQ(config.GetString(), "");
  EXPECT_EQ(config.GetMap(), nullptr);
  EXPECT_TRUE(config.GetArray().empty());
  EXPECT_EQ(config.GetArraySize(), 0);
  EXPECT_EQ(config.GetMapSize(), 0);
}

//==============================================================================
// GetArraySize / GetMapSize
//==============================================================================

TEST(ConfigTest, GetArraySizeNotArray) {
  EXPECT_EQ(Config::Int(42).GetArraySize(), 0);
  EXPECT_EQ(Config::String("x").GetArraySize(), 0);
}

TEST(ConfigTest, GetMapSizeNotMap) {
  EXPECT_EQ(Config::Int(42).GetMapSize(), 0);
}

//==============================================================================
// HasKey
//==============================================================================

TEST(ConfigTest, HasKeyExisting) {
  Config config = Config::Map();
  config.Set("key", Config::Int(1));
  EXPECT_TRUE(config.HasKey("key"));
}

TEST(ConfigTest, HasKeyMissing) {
  Config config = Config::Map();
  config.Set("key", Config::Int(1));
  EXPECT_FALSE(config.HasKey("other"));
}

TEST(ConfigTest, HasKeyNotMap) { EXPECT_FALSE(Config::Int(42).HasKey("key")); }

//==============================================================================
// Get(key) / Get(index)
//==============================================================================

TEST(ConfigTest, GetByKey) {
  Config config = Config::Map();
  config.Set("x", Config::Int(10));
  const Config* result = config.Get("x");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->GetInt(), 10);
}

TEST(ConfigTest, GetByKeyMissing) {
  Config config = Config::Map();
  EXPECT_EQ(config.Get("missing"), nullptr);
}

TEST(ConfigTest, GetByKeyNotMap) {
  EXPECT_EQ(Config::Int(42).Get("key"), nullptr);
}

TEST(ConfigTest, GetByIndex) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(10));
  arr.push_back(Config::Int(20));
  Config config = Config::Array(std::move(arr));
  const Config* result = config.Get(0);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->GetInt(), 10);
  result = config.Get(1);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->GetInt(), 20);
}

TEST(ConfigTest, GetByIndexOutOfRange) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(10));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(config.Get(-1), nullptr);
  EXPECT_EQ(config.Get(1), nullptr);
}

TEST(ConfigTest, GetByIndexNotArray) {
  EXPECT_EQ(Config::Int(42).Get(0), nullptr);
}

//==============================================================================
// Set(key, value) / Set(index, value)
//==============================================================================

TEST(ConfigTest, SetByKeyOnMap) {
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  config.Set("b", Config::Int(2));
  EXPECT_EQ(config.GetMapSize(), 2);
  EXPECT_EQ(config.GetInt("a"), 1);
  EXPECT_EQ(config.GetInt("b"), 2);
}

TEST(ConfigTest, SetByKeyReplacesExisting) {
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  config.Set("a", Config::Int(99));
  EXPECT_EQ(config.GetMapSize(), 1);
  EXPECT_EQ(config.GetInt("a"), 99);
}

TEST(ConfigTest, SetByKeyOnNonMapReplacesWithMap) {
  Config config = Config::Int(42);
  config.Set("key", Config::Bool(true));
  EXPECT_TRUE(config.IsMap());
  EXPECT_EQ(config.GetMapSize(), 1);
  EXPECT_TRUE(config.GetBool("key"));
}

TEST(ConfigTest, SetByIndexOnArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  Config config = Config::Array(std::move(arr));
  EXPECT_TRUE(config.Set(0, Config::Int(10)));
  EXPECT_EQ(config.GetInt(0), 10);
  EXPECT_EQ(config.GetInt(1), 2);
}

TEST(ConfigTest, SetByIndexAppends) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  Config config = Config::Array(std::move(arr));
  EXPECT_TRUE(config.Set(1, Config::Int(2)));
  EXPECT_EQ(config.GetArraySize(), 2);
  EXPECT_EQ(config.GetInt(1), 2);
}

TEST(ConfigTest, SetByIndexOutOfRange) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  Config config = Config::Array(std::move(arr));
  EXPECT_FALSE(config.Set(2, Config::Int(3)));
  EXPECT_FALSE(config.Set(-1, Config::Int(0)));
}

TEST(ConfigTest, SetByIndexZeroOnNonArray) {
  Config config = Config::Int(42);
  EXPECT_TRUE(config.Set(0, Config::Bool(true)));
  EXPECT_TRUE(config.IsArray());
  EXPECT_EQ(config.GetArraySize(), 1);
  EXPECT_TRUE(config.GetBool(0));
}

TEST(ConfigTest, SetByIndexNonZeroOnNonArray) {
  Config config = Config::Int(42);
  EXPECT_FALSE(config.Set(1, Config::Bool(true)));
  EXPECT_TRUE(config.IsInt());
}

//==============================================================================
// Scalar value accessors not interchangeable with array element
//==============================================================================

TEST(ConfigTest, ScalarNotInterchangeableWithArrayElement) {
  Config config = Config::Int(42);
  EXPECT_EQ(config.GetInt(), 42);
  EXPECT_EQ(config.GetInt(0), 0);
}

//==============================================================================
// Typed Get/Set by key and index
//==============================================================================

TEST(ConfigTest, GetSetBoolByKey) {
  Config config = Config::Map();
  config.SetBool("flag", true);
  EXPECT_TRUE(config.GetBool("flag"));
  EXPECT_FALSE(config.GetBool("missing"));
}

TEST(ConfigTest, GetSetBoolByIndex) {
  Config::ArrayValue arr;
  arr.push_back(Config::Bool(true));
  Config config = Config::Array(std::move(arr));
  EXPECT_TRUE(config.GetBool(0));
  EXPECT_FALSE(config.GetBool(1));
  EXPECT_TRUE(config.SetBool(1, false));
  EXPECT_FALSE(config.GetBool(1));
}

TEST(ConfigTest, GetSetIntByKey) {
  Config config = Config::Map();
  config.SetInt("val", 99);
  EXPECT_EQ(config.GetInt("val"), 99);
  EXPECT_EQ(config.GetInt("missing"), 0);
}

TEST(ConfigTest, GetSetIntByIndex) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(5));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(config.GetInt(0), 5);
  EXPECT_TRUE(config.SetInt(0, 10));
  EXPECT_EQ(config.GetInt(0), 10);
}

TEST(ConfigTest, GetSetFloatByKey) {
  Config config = Config::Map();
  config.SetFloat("val", 1.5);
  EXPECT_EQ(config.GetFloat("val"), 1.5);
  EXPECT_EQ(config.GetFloat("missing"), 0.0);
}

TEST(ConfigTest, GetSetFloatByIndex) {
  Config::ArrayValue arr;
  arr.push_back(Config::Float(2.5));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(config.GetFloat(0), 2.5);
  EXPECT_TRUE(config.SetFloat(0, 3.5));
  EXPECT_EQ(config.GetFloat(0), 3.5);
}

TEST(ConfigTest, GetSetStringByKey) {
  Config config = Config::Map();
  config.SetString("val", "abc");
  EXPECT_EQ(config.GetString("val"), "abc");
  EXPECT_EQ(config.GetString("missing"), "");
}

TEST(ConfigTest, GetSetStringByIndex) {
  Config::ArrayValue arr;
  arr.push_back(Config::String("hello"));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(config.GetString(0), "hello");
  EXPECT_TRUE(config.SetString(0, "world"));
  EXPECT_EQ(config.GetString(0), "world");
}

TEST(ConfigTest, GetMapByKey) {
  Config inner = Config::Map();
  inner.Set("x", Config::Int(1));
  Config config = Config::Map();
  config.Set("sub", std::move(inner));
  const Config::MapValue* map = config.GetMap("sub");
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(map->size(), 1);
  EXPECT_EQ(config.GetMap("missing"), nullptr);
}

TEST(ConfigTest, GetMapByIndex) {
  Config inner = Config::Map();
  inner.Set("x", Config::Int(1));
  Config::ArrayValue arr;
  arr.push_back(std::move(inner));
  Config config = Config::Array(std::move(arr));
  const Config::MapValue* map = config.GetMap(0);
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(config.GetMap(1), nullptr);
}

TEST(ConfigTest, GetArrayByKey) {
  Config::ArrayValue inner;
  inner.push_back(Config::Int(1));
  Config config = Config::Map();
  config.Set("arr", Config::Array(std::move(inner)));
  absl::Span<Config> span = config.GetArray("arr");
  EXPECT_EQ(span.size(), 1);
  EXPECT_TRUE(config.GetArray("missing").empty());
}

TEST(ConfigTest, GetArrayByIndex) {
  Config::ArrayValue inner;
  inner.push_back(Config::Int(1));
  Config::ArrayValue outer;
  outer.push_back(Config::Array(std::move(inner)));
  Config config = Config::Array(std::move(outer));
  absl::Span<Config> span = config.GetArray(0);
  EXPECT_EQ(span.size(), 1);
  EXPECT_TRUE(config.GetArray(1).empty());
}

TEST(ConfigTest, SetBoolReplacesValue) {
  Config config = Config::Int(42);
  config.SetBool(true);
  EXPECT_TRUE(config.IsBool());
  EXPECT_TRUE(config.GetBool());
}

TEST(ConfigTest, SetIntReplacesValue) {
  Config config = Config::Bool(true);
  config.SetInt(99);
  EXPECT_TRUE(config.IsInt());
  EXPECT_EQ(config.GetInt(), 99);
}

TEST(ConfigTest, SetFloatReplacesValue) {
  Config config = Config::Bool(true);
  config.SetFloat(1.5);
  EXPECT_TRUE(config.IsFloat());
  EXPECT_EQ(config.GetFloat(), 1.5);
}

TEST(ConfigTest, SetStringReplacesValue) {
  Config config = Config::Bool(true);
  config.SetString("hello");
  EXPECT_TRUE(config.IsString());
  EXPECT_EQ(config.GetString(), "hello");
}

TEST(ConfigTest, SetMapReplacesValue) {
  Config config = Config::Bool(true);
  Config::MapValue map;
  map.insert_or_assign("a", Config::Int(1));
  config.SetMap(std::move(map));
  EXPECT_TRUE(config.IsMap());
  EXPECT_EQ(config.GetMapSize(), 1);
}

TEST(ConfigTest, SetArrayReplacesValue) {
  Config config = Config::Bool(true);
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  config.SetArray(std::move(arr));
  EXPECT_TRUE(config.IsArray());
  EXPECT_EQ(config.GetArraySize(), 1);
}

//==============================================================================
// Map operations
//==============================================================================

TEST(ConfigTest, GetKeysReturnsAllKeys) {
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  config.Set("b", Config::Int(2));
  config.Set("c", Config::Int(3));
  std::vector<std::string> keys = config.GetKeys();
  EXPECT_EQ(keys.size(), 3);
  EXPECT_THAT(keys, UnorderedElementsAre("a", "b", "c"));
}

TEST(ConfigTest, GetKeysNotMapReturnsEmpty) {
  EXPECT_THAT(Config::Int(42).GetKeys(), IsEmpty());
}

TEST(ConfigTest, DeleteKeyExisting) {
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  config.Set("b", Config::Int(2));
  EXPECT_TRUE(config.DeleteKey("a"));
  EXPECT_EQ(config.GetMapSize(), 1);
  EXPECT_FALSE(config.HasKey("a"));
  EXPECT_TRUE(config.HasKey("b"));
}

TEST(ConfigTest, DeleteKeyMissing) {
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  EXPECT_FALSE(config.DeleteKey("b"));
  EXPECT_EQ(config.GetMapSize(), 1);
}

TEST(ConfigTest, DeleteKeyNotMap) {
  Config config = Config::Int(42);
  EXPECT_FALSE(config.DeleteKey("key"));
}

//==============================================================================
// Array operations
//==============================================================================

TEST(ConfigTest, ResizeArrayGrow) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  Config config = Config::Array(std::move(arr));
  config.ResizeArray(3);
  EXPECT_EQ(config.GetArraySize(), 3);
  EXPECT_EQ(config.GetInt(0), 1);
  EXPECT_FALSE(config.GetBool(1));
  EXPECT_FALSE(config.GetBool(2));
}

TEST(ConfigTest, ResizeArrayShrink) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  arr.push_back(Config::Int(3));
  Config config = Config::Array(std::move(arr));
  config.ResizeArray(1);
  EXPECT_EQ(config.GetArraySize(), 1);
  EXPECT_EQ(config.GetInt(0), 1);
}

TEST(ConfigTest, ResizeArrayFromNonArray) {
  Config config = Config::Int(42);
  config.ResizeArray(3);
  EXPECT_TRUE(config.IsArray());
  EXPECT_EQ(config.GetArraySize(), 3);
}

TEST(ConfigTest, ResizeArrayWithValue) {
  Config config = Config::Array();
  config.ResizeArray(3, Config::Int(99));
  EXPECT_EQ(config.GetArraySize(), 3);
  EXPECT_EQ(config.GetInt(0), 99);
  EXPECT_EQ(config.GetInt(1), 99);
  EXPECT_EQ(config.GetInt(2), 99);
}

TEST(ConfigTest, PopBackOnArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  Config config = Config::Array(std::move(arr));
  config.PopBack();
  EXPECT_EQ(config.GetArraySize(), 1);
  EXPECT_EQ(config.GetInt(0), 1);
}

TEST(ConfigTest, PopBackOnEmptyArray) {
  Config config = Config::Array();
  config.PopBack();
  EXPECT_EQ(config.GetArraySize(), 0);
}

TEST(ConfigTest, PopBackOnNonArray) {
  Config config = Config::Int(42);
  config.PopBack();
  EXPECT_EQ(config.GetInt(), 42);
}

TEST(ConfigTest, AppendToArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  Config config = Config::Array(std::move(arr));
  config.Append(Config::Int(2));
  EXPECT_EQ(config.GetArraySize(), 2);
  EXPECT_EQ(config.GetInt(0), 1);
  EXPECT_EQ(config.GetInt(1), 2);
}

TEST(ConfigTest, AppendToNonArray) {
  Config config = Config::Int(42);
  config.Append(Config::Int(1));
  EXPECT_TRUE(config.IsArray());
  EXPECT_EQ(config.GetArraySize(), 1);
  EXPECT_EQ(config.GetInt(0), 1);
}

TEST(ConfigTest, AppendTypedHelpers) {
  Config config = Config::Array();
  config.AppendBool(true);
  config.AppendInt(42);
  config.AppendFloat(1.5);
  config.AppendString("hello");
  EXPECT_EQ(config.GetArraySize(), 4);
  EXPECT_TRUE(config.GetBool(0));
  EXPECT_EQ(config.GetInt(1), 42);
  EXPECT_EQ(config.GetFloat(2), 1.5);
  EXPECT_EQ(config.GetString(3), "hello");
}

//==============================================================================
// AsBool coercion
//==============================================================================

TEST(ConfigTest, AsBoolFromBool) {
  EXPECT_TRUE(Config::Bool(true).AsBool());
  EXPECT_FALSE(Config::Bool(false).AsBool());
}

TEST(ConfigTest, AsBoolFromInt) {
  EXPECT_TRUE(Config::Int(1).AsBool());
  EXPECT_TRUE(Config::Int(-1).AsBool());
  EXPECT_FALSE(Config::Int(0).AsBool());
}

TEST(ConfigTest, AsBoolFromFloat) {
  EXPECT_TRUE(Config::Float(1.0).AsBool());
  EXPECT_TRUE(Config::Float(-0.5).AsBool());
  EXPECT_FALSE(Config::Float(0.0).AsBool());
}

TEST(ConfigTest, AsBoolFromString) {
  EXPECT_TRUE(Config::String("hello").AsBool());
  EXPECT_TRUE(Config::String("1").AsBool());
  EXPECT_TRUE(Config::String("TRUE").AsBool());
  EXPECT_FALSE(Config::String("").AsBool());
  EXPECT_FALSE(Config::String("false").AsBool());
  EXPECT_FALSE(Config::String("FALSE").AsBool());
  EXPECT_FALSE(Config::String("False").AsBool());
  EXPECT_FALSE(Config::String("0").AsBool());
  EXPECT_FALSE(Config::String("0.0").AsBool());
}

TEST(ConfigTest, AsBoolFromMapArray) {
  EXPECT_FALSE(Config::Map().AsBool());
  EXPECT_FALSE(Config::Array().AsBool());
}

TEST(ConfigTest, AsBoolFromNone) {
  EXPECT_FALSE(Config::None().AsBool());
}

TEST(ConfigTest, AsBoolByKey) {
  Config config = Config::Map();
  config.Set("flag", Config::Bool(true));
  EXPECT_TRUE(config.AsBool("flag"));
  EXPECT_FALSE(config.AsBool("missing"));
}

TEST(ConfigTest, AsBoolByIndex) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  Config config = Config::Array(std::move(arr));
  EXPECT_TRUE(config.AsBool(0));
  EXPECT_FALSE(config.AsBool(1));
}

//==============================================================================
// AsInt coercion
//==============================================================================

TEST(ConfigTest, AsIntFromBool) {
  EXPECT_EQ(Config::Bool(true).AsInt(), 1);
  EXPECT_EQ(Config::Bool(false).AsInt(), 0);
}

TEST(ConfigTest, AsIntFromInt) {
  EXPECT_EQ(Config::Int(42).AsInt(), 42);
  EXPECT_EQ(Config::Int(-100).AsInt(), -100);
}

TEST(ConfigTest, AsIntFromFloat) {
  EXPECT_EQ(Config::Float(3.9).AsInt(), 3);
  EXPECT_EQ(Config::Float(-3.9).AsInt(), -3);
}

TEST(ConfigTest, AsIntFromFloatClamped) {
  EXPECT_EQ(Config::Float(1e30).AsInt(), std::numeric_limits<int64_t>::max());
  EXPECT_EQ(Config::Float(-1e30).AsInt(), std::numeric_limits<int64_t>::min());
}

TEST(ConfigTest, AsIntFromNanFloat) {
  int64_t result =
      Config::Float(std::numeric_limits<double>::quiet_NaN()).AsInt();
  EXPECT_EQ(result, 0);
}

TEST(ConfigTest, AsIntFromInfFloat) {
  EXPECT_EQ(Config::Float(std::numeric_limits<double>::infinity()).AsInt(),
            std::numeric_limits<int64_t>::max());
  EXPECT_EQ(Config::Float(-std::numeric_limits<double>::infinity()).AsInt(),
            std::numeric_limits<int64_t>::min());
}

TEST(ConfigTest, AsIntFromString) {
  EXPECT_EQ(Config::String("42").AsInt(), 42);
  EXPECT_EQ(Config::String("-100").AsInt(), -100);
  EXPECT_EQ(Config::String("abc").AsInt(), 0);
  EXPECT_EQ(Config::String("").AsInt(), 0);
}

TEST(ConfigTest, AsIntFromMapArray) {
  EXPECT_EQ(Config::Map().AsInt(), 0);
  EXPECT_EQ(Config::Array().AsInt(), 0);
}

TEST(ConfigTest, AsIntFromNone) {
  EXPECT_EQ(Config::None().AsInt(), 0);
}

TEST(ConfigTest, AsIntByKey) {
  Config config = Config::Map();
  config.Set("val", Config::Int(42));
  EXPECT_EQ(config.AsInt("val"), 42);
  EXPECT_EQ(config.AsInt("missing"), 0);
}

TEST(ConfigTest, AsIntByIndex) {
  Config::ArrayValue arr;
  arr.push_back(Config::Bool(true));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(config.AsInt(0), 1);
  EXPECT_EQ(config.AsInt(1), 0);
}

//==============================================================================
// AsFloat coercion
//==============================================================================

TEST(ConfigTest, AsFloatFromBool) {
  EXPECT_EQ(Config::Bool(true).AsFloat(), 1.0);
  EXPECT_EQ(Config::Bool(false).AsFloat(), 0.0);
}

TEST(ConfigTest, AsFloatFromInt) { EXPECT_EQ(Config::Int(42).AsFloat(), 42.0); }

TEST(ConfigTest, AsFloatFromFloat) {
  EXPECT_EQ(Config::Float(3.14).AsFloat(), 3.14);
}

TEST(ConfigTest, AsFloatFromString) {
  EXPECT_EQ(Config::String("3.14").AsFloat(), 3.14);
  EXPECT_EQ(Config::String("abc").AsFloat(), 0.0);
  EXPECT_EQ(Config::String("").AsFloat(), 0.0);
}

TEST(ConfigTest, AsFloatFromMapArray) {
  EXPECT_EQ(Config::Map().AsFloat(), 0.0);
  EXPECT_EQ(Config::Array().AsFloat(), 0.0);
}

TEST(ConfigTest, AsFloatFromNone) {
  EXPECT_EQ(Config::None().AsFloat(), 0.0);
}

TEST(ConfigTest, AsFloatByKey) {
  Config config = Config::Map();
  config.Set("val", Config::Float(1.5));
  EXPECT_EQ(config.AsFloat("val"), 1.5);
  EXPECT_EQ(config.AsFloat("missing"), 0.0);
}

TEST(ConfigTest, AsFloatByIndex) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(10));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(config.AsFloat(0), 10.0);
  EXPECT_EQ(config.AsFloat(1), 0.0);
}

//==============================================================================
// AsString coercion
//==============================================================================

TEST(ConfigTest, AsStringFromBool) {
  EXPECT_EQ(Config::Bool(true).AsString(), "true");
  EXPECT_EQ(Config::Bool(false).AsString(), "false");
}

TEST(ConfigTest, AsStringFromInt) {
  EXPECT_EQ(Config::Int(42).AsString(), "42");
  EXPECT_EQ(Config::Int(-100).AsString(), "-100");
}

TEST(ConfigTest, AsStringFromFloat) {
  std::string result = Config::Float(1.5).AsString();
  EXPECT_FALSE(result.empty());
}

TEST(ConfigTest, AsStringFromString) {
  EXPECT_EQ(Config::String("hello").AsString(), "hello");
}

TEST(ConfigTest, AsStringFromMapArray) {
  EXPECT_EQ(Config::Map().AsString(), "");
  EXPECT_EQ(Config::Array().AsString(), "");
}

TEST(ConfigTest, AsStringFromNone) {
  EXPECT_EQ(Config::None().AsString(), "");
}

TEST(ConfigTest, AsStringByKey) {
  Config config = Config::Map();
  config.Set("val", Config::Int(42));
  EXPECT_EQ(config.AsString("val"), "42");
  EXPECT_EQ(config.AsString("missing"), "");
}

TEST(ConfigTest, AsStringByIndex) {
  Config::ArrayValue arr;
  arr.push_back(Config::Bool(true));
  Config config = Config::Array(std::move(arr));
  EXPECT_EQ(config.AsString(0), "true");
  EXPECT_EQ(config.AsString(1), "");
}

//==============================================================================
// AsMap coercion
//==============================================================================

TEST(ConfigTest, AsMapFromScalar) {
  Config::MapValue result = Config::Int(42).AsMap();
  EXPECT_EQ(result.size(), 1);
  auto it = result.find("");
  ASSERT_NE(it, result.end());
  EXPECT_EQ(it->second.GetInt(), 42);
}

TEST(ConfigTest, AsMapFromMap) {
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  config.Set("b", Config::Int(2));
  Config::MapValue result = config.AsMap();
  EXPECT_EQ(result.size(), 2);
}

TEST(ConfigTest, AsMapFromArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(10));
  arr.push_back(Config::Int(20));
  arr.push_back(Config::Int(30));
  Config config = Config::Array(std::move(arr));
  Config::MapValue result = config.AsMap();
  EXPECT_EQ(result.size(), 3);
  EXPECT_EQ(result.at("0").GetInt(), 10);
  EXPECT_EQ(result.at("1").GetInt(), 20);
  EXPECT_EQ(result.at("2").GetInt(), 30);
}

//==============================================================================
// AsArray coercion
//==============================================================================

TEST(ConfigTest, AsArrayFromScalar) {
  Config::ArrayValue result = Config::Int(42).AsArray();
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].GetInt(), 42);
}

TEST(ConfigTest, AsArrayFromArray) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  Config config = Config::Array(std::move(arr));
  Config::ArrayValue result = config.AsArray();
  EXPECT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].GetInt(), 1);
  EXPECT_EQ(result[1].GetInt(), 2);
}

TEST(ConfigTest, AsArrayFromMapOrderedByKey) {
  Config config = Config::Map();
  config.Set("b", Config::Int(2));
  config.Set("a", Config::Int(1));
  config.Set("c", Config::Int(3));
  Config::ArrayValue result = config.AsArray();
  EXPECT_EQ(result.size(), 3);
  EXPECT_EQ(result[0].GetInt(), 1);
  EXPECT_EQ(result[1].GetInt(), 2);
  EXPECT_EQ(result[2].GetInt(), 3);
}

TEST(ConfigTest, AsMapFromNone) {
  Config::MapValue result = Config::None().AsMap();
  EXPECT_TRUE(result.empty());
}

TEST(ConfigTest, AsArrayFromNone) {
  Config::ArrayValue result = Config::None().AsArray();
  EXPECT_TRUE(result.empty());
}

//==============================================================================
// SetBool/SetInt/SetFloat/SetString by key on non-map
//==============================================================================

TEST(ConfigTest, SetBoolByKeyOnNonMap) {
  Config config = Config::Int(42);
  config.SetBool("flag", true);
  EXPECT_TRUE(config.IsMap());
  EXPECT_TRUE(config.GetBool("flag"));
}

TEST(ConfigTest, SetIntByKeyOnNonMap) {
  Config config = Config::Bool(true);
  config.SetInt("val", 99);
  EXPECT_TRUE(config.IsMap());
  EXPECT_EQ(config.GetInt("val"), 99);
}

TEST(ConfigTest, SetFloatByKeyOnNonMap) {
  Config config = Config::Bool(true);
  config.SetFloat("val", 1.5);
  EXPECT_TRUE(config.IsMap());
  EXPECT_EQ(config.GetFloat("val"), 1.5);
}

TEST(ConfigTest, SetStringByKeyOnNonMap) {
  Config config = Config::Bool(true);
  config.SetString("val", "hello");
  EXPECT_TRUE(config.IsMap());
  EXPECT_EQ(config.GetString("val"), "hello");
}

//==============================================================================
// SetBool/SetInt/SetFloat/SetString by index failures
//==============================================================================

TEST(ConfigTest, SetBoolByIndexFails) {
  Config config = Config::Int(42);
  EXPECT_FALSE(config.SetBool(1, true));
}

TEST(ConfigTest, SetIntByIndexFails) {
  Config config = Config::Int(42);
  EXPECT_FALSE(config.SetInt(1, 99));
}

TEST(ConfigTest, SetFloatByIndexFails) {
  Config config = Config::Int(42);
  EXPECT_FALSE(config.SetFloat(1, 1.5));
}

TEST(ConfigTest, SetStringByIndexFails) {
  Config config = Config::Int(42);
  EXPECT_FALSE(config.SetString(1, "hello"));
}

//==============================================================================
// GetMap / GetArray direct access
//==============================================================================

TEST(ConfigTest, GetMapDirect) {
  Config config = Config::Map();
  config.Set("a", Config::Int(1));
  const Config::MapValue* map = config.GetMap();
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(map->size(), 1);
}

TEST(ConfigTest, GetArrayDirect) {
  Config::ArrayValue arr;
  arr.push_back(Config::Int(1));
  arr.push_back(Config::Int(2));
  Config config = Config::Array(std::move(arr));
  absl::Span<Config> span = config.GetArray();
  EXPECT_EQ(span.size(), 2);
  EXPECT_EQ(span[0].GetInt(), 1);
  EXPECT_EQ(span[1].GetInt(), 2);
}

}  // namespace
}  // namespace gb
