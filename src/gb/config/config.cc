// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/config/config.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"

namespace gb {

//==============================================================================
// Type accessors
//==============================================================================

Config::Type Config::GetType() const {
  return static_cast<Type>(value_.index());
}

//==============================================================================
// Value accessors
//==============================================================================

int Config::GetArraySize() const {
  auto* arr = std::get_if<ArrayValue>(&value_);
  return arr != nullptr ? static_cast<int>(arr->size()) : 0;
}

int Config::GetMapSize() const {
  auto* map = std::get_if<MapValue>(&value_);
  return map != nullptr ? static_cast<int>(map->size()) : 0;
}

bool Config::HasKey(std::string_view key) const {
  auto* map = std::get_if<MapValue>(&value_);
  return map != nullptr && map->contains(key);
}

const Config* Config::Get(std::string_view key) const {
  auto* map = std::get_if<MapValue>(&value_);
  if (map == nullptr) {
    return nullptr;
  }
  auto it = map->find(key);
  return it != map->end() ? &it->second : nullptr;
}

const Config* Config::Get(int index) const {
  auto* arr = std::get_if<ArrayValue>(&value_);
  if (arr == nullptr || index < 0 || index >= static_cast<int>(arr->size())) {
    return nullptr;
  }
  return &(*arr)[index];
}

void Config::Set(std::string_view key, Config value) {
  auto* map = std::get_if<MapValue>(&value_);
  if (map == nullptr) {
    MapValue new_map;
    new_map.insert_or_assign(std::string(key), std::move(value));
    value_ = std::move(new_map);
  } else {
    map->insert_or_assign(std::string(key), std::move(value));
  }
}

bool Config::Set(int index, Config value) {
  auto* arr = std::get_if<ArrayValue>(&value_);
  if (arr == nullptr) {
    if (index != 0) {
      return false;
    }
    ArrayValue new_arr;
    new_arr.push_back(std::move(value));
    value_ = std::move(new_arr);
    return true;
  }
  if (index < 0 || index > static_cast<int>(arr->size())) {
    return false;
  }
  if (index == static_cast<int>(arr->size())) {
    arr->push_back(std::move(value));
  } else {
    (*arr)[index] = std::move(value);
  }
  return true;
}

// Bool accessors

bool Config::GetBool() const {
  auto* val = std::get_if<bool>(&value_);
  return val != nullptr ? *val : false;
}

bool Config::GetBool(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->GetBool() : false;
}

bool Config::GetBool(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->GetBool() : false;
}

void Config::SetBool(bool value) { value_ = value; }

void Config::SetBool(std::string_view key, bool value) {
  Set(key, Bool(value));
}

bool Config::SetBool(int index, bool value) { return Set(index, Bool(value)); }

// Int accessors

int64_t Config::GetInt() const {
  auto* val = std::get_if<int64_t>(&value_);
  return val != nullptr ? *val : 0;
}

int64_t Config::GetInt(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->GetInt() : 0;
}

int64_t Config::GetInt(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->GetInt() : 0;
}

void Config::SetInt(int64_t value) { value_ = value; }

void Config::SetInt(std::string_view key, int64_t value) {
  Set(key, Int(value));
}

bool Config::SetInt(int index, int64_t value) { return Set(index, Int(value)); }

// UInt accessors

uint64_t Config::GetUInt() const {
  auto* val = std::get_if<uint64_t>(&value_);
  return val != nullptr ? *val : 0;
}

uint64_t Config::GetUInt(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->GetUInt() : 0;
}

uint64_t Config::GetUInt(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->GetUInt() : 0;
}

void Config::SetUInt(uint64_t value) { value_ = value; }

void Config::SetUInt(std::string_view key, uint64_t value) {
  Set(key, UInt(value));
}

bool Config::SetUInt(int index, uint64_t value) {
  return Set(index, UInt(value));
}

// Float accessors

double Config::GetFloat() const {
  auto* val = std::get_if<double>(&value_);
  return val != nullptr ? *val : 0.0;
}

double Config::GetFloat(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->GetFloat() : 0.0;
}

double Config::GetFloat(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->GetFloat() : 0.0;
}

void Config::SetFloat(double value) { value_ = value; }

void Config::SetFloat(std::string_view key, double value) {
  Set(key, Float(value));
}

bool Config::SetFloat(int index, double value) {
  return Set(index, Float(value));
}

// String accessors

std::string_view Config::GetString() const {
  auto* val = std::get_if<std::string>(&value_);
  return val != nullptr ? std::string_view(*val) : std::string_view();
}

std::string_view Config::GetString(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->GetString() : std::string_view();
}

std::string_view Config::GetString(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->GetString() : std::string_view();
}

void Config::SetString(std::string value) { value_ = std::move(value); }

void Config::SetString(std::string_view key, std::string value) {
  Set(key, String(std::move(value)));
}

bool Config::SetString(int index, std::string value) {
  return Set(index, String(std::move(value)));
}

// Map accessors

const Config::MapValue* Config::GetMap() const {
  return std::get_if<MapValue>(&value_);
}

const Config::MapValue* Config::GetMap(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->GetMap() : nullptr;
}

const Config::MapValue* Config::GetMap(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->GetMap() : nullptr;
}

void Config::SetMap(MapValue value) { value_ = std::move(value); }

// Array accessors

absl::Span<Config> Config::GetArray() const {
  auto* arr = std::get_if<ArrayValue>(&value_);
  if (arr == nullptr) {
    return {};
  }
  return absl::Span<Config>(const_cast<Config*>(arr->data()), arr->size());
}

absl::Span<Config> Config::GetArray(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->GetArray() : absl::Span<Config>();
}

absl::Span<Config> Config::GetArray(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->GetArray() : absl::Span<Config>();
}

void Config::SetArray(ArrayValue value) { value_ = std::move(value); }

//==============================================================================
// Map operations
//==============================================================================

std::vector<std::string> Config::GetKeys() const {
  auto* map = std::get_if<MapValue>(&value_);
  if (map == nullptr) {
    return {};
  }
  std::vector<std::string> keys;
  keys.reserve(map->size());
  for (const auto& pair : *map) {
    keys.push_back(pair.first);
  }
  return keys;
}

bool Config::DeleteKey(std::string_view key) {
  auto* map = std::get_if<MapValue>(&value_);
  if (map == nullptr) {
    return false;
  }
  auto it = map->find(key);
  if (it == map->end()) {
    return false;
  }
  map->erase(it);
  return true;
}

//==============================================================================
// Array operations
//==============================================================================

void Config::ResizeArray(int new_size) { ResizeArray(new_size, Bool(false)); }

void Config::ResizeArray(int new_size, const Config& value) {
  auto* arr = std::get_if<ArrayValue>(&value_);
  if (arr == nullptr) {
    ArrayValue new_arr(new_size, value);
    value_ = std::move(new_arr);
    return;
  }
  arr->resize(new_size, value);
}

void Config::PopBack() {
  auto* arr = std::get_if<ArrayValue>(&value_);
  if (arr == nullptr || arr->empty()) {
    return;
  }
  arr->pop_back();
}

void Config::Append(Config value) {
  auto* arr = std::get_if<ArrayValue>(&value_);
  if (arr == nullptr) {
    ArrayValue new_arr;
    new_arr.push_back(std::move(value));
    value_ = std::move(new_arr);
    return;
  }
  arr->push_back(std::move(value));
}

//==============================================================================
// Value coercion functions
//==============================================================================

bool Config::AsBool() const {
  return std::visit(
      [](const auto& val) -> bool {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>) {
          return val;
        } else if constexpr (std::is_same_v<T, int64_t>) {
          return val != 0;
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          return val != 0;
        } else if constexpr (std::is_same_v<T, double>) {
          return val != 0.0;
        } else if constexpr (std::is_same_v<T, std::string>) {
          if (val.empty() || val == "0" || val == "0.0") {
            return false;
          }
          std::string lower = absl::AsciiStrToLower(val);
          return lower != "false";
        } else {
          return false;
        }
      },
      value_);
}

bool Config::AsBool(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->AsBool() : false;
}

bool Config::AsBool(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->AsBool() : false;
}

int64_t Config::AsInt() const {
  return std::visit(
      [](const auto& val) -> int64_t {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>) {
          return val ? 1 : 0;
        } else if constexpr (std::is_same_v<T, int64_t>) {
          return val;
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          return static_cast<int64_t>(val);
        } else if constexpr (std::is_same_v<T, double>) {
          if (std::isnan(val) || std::isinf(val)) {
            if (std::isnan(val)) return 0;
            return val > 0 ? std::numeric_limits<int64_t>::max()
                           : std::numeric_limits<int64_t>::min();
          }
          if (val >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
            return std::numeric_limits<int64_t>::max();
          }
          if (val <= static_cast<double>(std::numeric_limits<int64_t>::min())) {
            return std::numeric_limits<int64_t>::min();
          }
          return static_cast<int64_t>(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
          char* end = nullptr;
          int64_t result = std::strtoll(val.c_str(), &end, 10);
          return (end != val.c_str() && *end == '\0') ? result : 0;
        } else {
          return 0;
        }
      },
      value_);
}

int64_t Config::AsInt(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->AsInt() : 0;
}

int64_t Config::AsInt(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->AsInt() : 0;
}

uint64_t Config::AsUInt() const {
  return std::visit(
      [](const auto& val) -> uint64_t {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>) {
          return val ? 1 : 0;
        } else if constexpr (std::is_same_v<T, int64_t>) {
          return static_cast<uint64_t>(val);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          return val;
        } else if constexpr (std::is_same_v<T, double>) {
          if (std::isnan(val) || val < 0.0) {
            if (std::isnan(val)) return 0;
            return 0;
          }
          if (std::isinf(val)) {
            return std::numeric_limits<uint64_t>::max();
          }
          if (val >=
              static_cast<double>(std::numeric_limits<uint64_t>::max())) {
            return std::numeric_limits<uint64_t>::max();
          }
          return static_cast<uint64_t>(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
          char* end = nullptr;
          uint64_t result = std::strtoull(val.c_str(), &end, 10);
          return (end != val.c_str() && *end == '\0') ? result : 0;
        } else {
          return 0;
        }
      },
      value_);
}

uint64_t Config::AsUInt(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->AsUInt() : 0;
}

uint64_t Config::AsUInt(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->AsUInt() : 0;
}

double Config::AsFloat() const {
  return std::visit(
      [](const auto& val) -> double {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>) {
          return val ? 1.0 : 0.0;
        } else if constexpr (std::is_same_v<T, int64_t>) {
          return static_cast<double>(val);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          return static_cast<double>(val);
        } else if constexpr (std::is_same_v<T, double>) {
          return val;
        } else if constexpr (std::is_same_v<T, std::string>) {
          char* end = nullptr;
          double result = std::strtod(val.c_str(), &end);
          return (end != val.c_str() && *end == '\0') ? result : 0.0;
        } else {
          return 0.0;
        }
      },
      value_);
}

double Config::AsFloat(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->AsFloat() : 0.0;
}

double Config::AsFloat(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->AsFloat() : 0.0;
}

std::string Config::AsString() const {
  return std::visit(
      [](const auto& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>) {
          return val ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int64_t>) {
          return absl::StrCat(val);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          return absl::StrCat(val);
        } else if constexpr (std::is_same_v<T, double>) {
          return absl::StrCat(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
          return val;
        } else {
          return "";
        }
      },
      value_);
}

std::string Config::AsString(std::string_view key) const {
  auto* config = Get(key);
  return config != nullptr ? config->AsString() : "";
}

std::string Config::AsString(int index) const {
  auto* config = Get(index);
  return config != nullptr ? config->AsString() : "";
}

Config::MapValue Config::AsMap() const {
  return std::visit(
      [](const auto& val) -> MapValue {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, MapValue>) {
          return val;
        } else if constexpr (std::is_same_v<T, ArrayValue>) {
          MapValue result;
          for (size_t i = 0; i < val.size(); ++i) {
            result.insert_or_assign(absl::StrCat(i), val[i]);
          }
          return result;
        } else {
          MapValue result;
          result.insert_or_assign("", Config(Value(val)));
          return result;
        }
      },
      value_);
}

Config::ArrayValue Config::AsArray() const {
  return std::visit(
      [](const auto& val) -> ArrayValue {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, ArrayValue>) {
          return val;
        } else if constexpr (std::is_same_v<T, MapValue>) {
          std::vector<std::string> keys;
          keys.reserve(val.size());
          for (const auto& pair : val) {
            keys.push_back(pair.first);
          }
          std::sort(keys.begin(), keys.end());
          ArrayValue result;
          result.reserve(val.size());
          for (const auto& key : keys) {
            result.push_back(val.at(key));
          }
          return result;
        } else {
          ArrayValue result;
          result.push_back(Config(Value(val)));
          return result;
        }
      },
      value_);
}

}  // namespace gb