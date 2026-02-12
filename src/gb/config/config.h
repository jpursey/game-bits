// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONFIG_CONFIG_H_
#define GB_CONFIG_CONFIG_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"

namespace gb {

// Represents a configuration value, which can be a bool, int, float,
// string, or a map or array of other config values.
class Config {
 public:
  enum class Type {
    kBool,    // A boolean value.
    kInt,     // A 64-bit integer value.
    kFloat,   // A 64-bit floating point value.
    kString,  // A string value.
    kMap,     // A map of string keys to values (unordered).
    kArray,   // An array of values.
  };

  // Underlying value for a config value. The type of the value is determined by
  // GetType().
  using MapValue = absl::flat_hash_map<std::string, Config>;
  using ArrayValue = std::vector<Config>;
  using Value =
      std::variant<bool, int64_t, double, std::string, MapValue, ArrayValue>;

  //----------------------------------------------------------------------------
  // Factory functions
  //----------------------------------------------------------------------------

  // Creates a config value with the specified value.
  static Config Bool(bool value) { return Config(value); }
  static Config Int(int64_t value) { return Config(value); }
  static Config Float(double value) { return Config(value); }
  static Config String(std::string value) { return Config(std::move(value)); }
  static Config String(std::string_view value) {
    return Config(std::string(value));
  }
  static Config String(const char* value) { return Config(std::string(value)); }
  static Config Map() { return Config(MapValue()); }
  static Config Map(MapValue value) { return Config(std::move(value)); }
  static Config Map(absl::Span<const MapValue::value_type> value) {
    return Config(MapValue(value.begin(), value.end()));
  }
  template <typename Iterator>
  static Config Map(Iterator begin, Iterator end) {
    return Config(MapValue(begin, end));
  }
  static Config Array() { return Config(ArrayValue()); }
  static Config Array(ArrayValue value) { return Config(std::move(value)); }
  static Config Array(absl::Span<const Config> value) {
    return Config(ArrayValue(value.begin(), value.end()));
  }
  template <typename Iterator>
  static Config Array(Iterator begin, Iterator end) {
    return Config(ArrayValue(begin, end));
  }

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  Config(const Config& other) = default;
  Config(Config&& other) = default;
  Config& operator=(const Config& other) = default;
  Config& operator=(Config&& other) = default;
  ~Config() = default;

  //----------------------------------------------------------------------------
  // Type accessors
  //----------------------------------------------------------------------------

  // Returns the type of this config value.
  Type GetType() const;

  // Tests for the specific type of this config value.
  bool IsBool() const { return std::holds_alternative<bool>(value_); }
  bool IsInt() const { return std::holds_alternative<int64_t>(value_); }
  bool IsFloat() const { return std::holds_alternative<double>(value_); }
  bool IsString() const { return std::holds_alternative<std::string>(value_); }
  bool IsMap() const { return std::holds_alternative<MapValue>(value_); }
  bool IsArray() const { return std::holds_alternative<ArrayValue>(value_); }

  //----------------------------------------------------------------------------
  // Value accessors
  //
  // These provide type-specific access to the underlying value of this
  // configuration.
  //
  // Values can also be requested by key or index, which will return the value
  // at the key (if it is a map) or index (if it is an array). If the requested
  // value does not exist or is of the incorrect type, these will return the
  // default value for that type:
  //   - bool: false
  //   - int / float: 0
  //   - string: ""
  //   - map: nullptr
  //   - array: empty span
  // Note that a scalar value is not interchangeable with the first element of
  // an array. For example GetInt() and GetInt(0) will never refer to the same
  // underlying value.
  //
  // Values may also be set which has the following behavior:
  //   - If set without a key or index, replaces the entire config with the new
  //     value.
  //   - If set with a key, sets the value at the key if this is a map, or
  //     replaces the entire config with a map containing the key and value.
  //   - If set with an index, sets the value at the index if this is an array.
  //     If the index is the current count, then this will append the value to
  //     the array. If the index is greater than the current count or less than
  //     zero, then this will fail. If the current config is not an array, then
  //     the only valid index is zero, which will replace the entire config with
  //     an array.
  //----------------------------------------------------------------------------

  // Returns the number of values in this array, or zero this is not an array.
  int GetArraySize() const;

  // Returns the number of key/value pairs in this map, or zero if this is not a
  // map.
  int GetMapSize() const;

  // Returns true if this is a map and contains the specified key.
  bool HasKey(std::string_view key) const;

  // Returns the config value at the specified key or index. Returns nullptr if
  // the Config is not the right type or does not exist at the specified key or
  // index.
  const Config* Get(std::string_view key) const;
  const Config* Get(int index) const;

  // Sets the value of this config to the specified value at the key or index.
  // Returns false if setting a value by index and it is not within the range
  // [0,GetArraySize()].
  void Set(std::string_view key, Config value);
  bool Set(int index, Config value);

  // Returns false if this is not a bool, or it is not found.
  bool GetBool() const;
  bool GetBool(std::string_view key) const;
  bool GetBool(int index) const;

  // Sets the value of this config to a bool, or a map / array containing a
  // bool. Returns false if setting a value by index and it is not within the
  // range [0,GetArraySize()].
  void SetBool(bool value);
  void SetBool(std::string_view key, bool value);
  bool SetBool(int index, bool value);

  // Returns 0 if this is not an int.
  int64_t GetInt() const;
  int64_t GetInt(std::string_view key) const;
  int64_t GetInt(int index) const;

  // Sets the value of this config to an int, or a map / array containing an
  // int. Returns false if setting a value by index and it is not within the
  // range [0,GetArraySize()].
  void SetInt(int64_t value);
  void SetInt(std::string_view key, int64_t value);
  bool SetInt(int index, int64_t value);

  // Returns 0 if this is not a float.
  double GetFloat() const;
  double GetFloat(std::string_view key) const;
  double GetFloat(int index) const;

  // Sets the value of this config to a float. Returns false if setting a value
  // by index and it is not within the range [0,GetArraySize()].
  void SetFloat(double value);
  void SetFloat(std::string_view key, double value);
  bool SetFloat(int index, double value);

  // Returns empty string if this is not a string.
  std::string_view GetString() const;
  std::string_view GetString(std::string_view key) const;
  std::string_view GetString(int index) const;

  // Sets the value of this config to a string. Returns false if setting a value
  // by index and it is not within the range [0,GetArraySize()].
  void SetString(std::string value);
  void SetString(std::string_view key, std::string value);
  bool SetString(int index, std::string value);

  // Returns nullptr if this is not a map.
  const MapValue* GetMap() const;
  const MapValue* GetMap(std::string_view key) const;
  const MapValue* GetMap(int index) const;

  // Sets the value of this config to a map.
  void SetMap(MapValue value);
  void SetMap(absl::Span<const MapValue::value_type> value) {
    SetMap(MapValue(value.begin(), value.end()));
  }
  template <typename Iterator>
  void SetMap(Iterator begin, Iterator end) {
    SetMap(MapValue(begin, end));
  }

  // Returns empty span if this is not an array.
  absl::Span<Config> GetArray() const;
  absl::Span<Config> GetArray(std::string_view key) const;
  absl::Span<Config> GetArray(int index) const;

  // Sets the value of this config to an array.
  void SetArray(ArrayValue value);
  void SetArray(absl::Span<const Config> value) {
    SetArray(ArrayValue(value.begin(), value.end()));
  }
  template <typename Iterator>
  void SetArray(Iterator begin, Iterator end) {
    SetArray(ArrayValue(begin, end));
  }

  //----------------------------------------------------------------------------
  // Map operations
  //
  // These functions provide additional functionality for map values.
  //----------------------------------------------------------------------------

  // Returns a vector of the keys in this map. If this is not a map, then this
  // returns an empty vector.
  std::vector<std::string> GetKeys() const;

  // Deletes the value at the specified key. Returns true if the key was found
  // and was deleted. If this is not a map or the key was not found, then this
  // returns false.
  bool DeleteKey(std::string_view key);

  //----------------------------------------------------------------------------
  // Array operations
  //
  // These functions provide additional functionality for array values.
  //----------------------------------------------------------------------------

  // Resizes this array to the specified size. If the config is not currently
  // array, it becomes an array of the specified size replacing the existing
  // value. If the new size is greater than the current size, then new elements
  // will be default initialized to Config::Bool(false) if not specified.
  void ResizeArray(int new_size);
  void ResizeArray(int new_size, const Config& value);

  // Removes the last element of the array. If the config is not an array or the
  // array is already empty, this does nothing.
  void PopBack();

  // Appends the specified value to the end of this array. If the config is not
  // an array, it is replaced to be an array with only the newly appended
  // value.
  void Append(Config value);
  void AppendBool(bool value) { Append(Bool(value)); }
  void AppendInt(int64_t value) { Append(Int(value)); }
  void AppendFloat(double value) { Append(Float(value)); }
  void AppendString(std::string value) { Append(String(std::move(value))); }
  void AppendString(std::string_view value) { Append(String(value)); }
  void AppendString(const char* value) { Append(String(value)); }
  void AppendMap(MapValue value) { Append(Map(std::move(value))); }
  void AppendMap(absl::Span<const MapValue::value_type> value) {
    Append(Map(value));
  }
  template <typename Iterator>
  void AppendMap(Iterator begin, Iterator end) {
    Append(Map(begin, end));
  }
  void AppendArray(ArrayValue value) { Append(Array(std::move(value))); }
  void AppendArray(absl::Span<const Config> value) { Append(Array(value)); }
  template <typename Iterator>
  void AppendArray(Iterator begin, Iterator end) {
    Append(Array(begin, end));
  }

  //----------------------------------------------------------------------------
  // Value coersion functions
  //
  // These functions attempt to coerce the value to the specified type.
  //----------------------------------------------------------------------------

  // Returns the following based on the type:
  //   - bool: The bool value.
  //   - int: False if the int value is zero, true otherwise.
  //   - float: False if the float value is zero, true otherwise.
  //   - string: False if the string is empty, "false" (case-insensitive),
  //     "0", or "0.0"; true otherwise.
  //   - map / array: False
  bool AsBool() const;
  bool AsBool(std::string_view key) const;
  bool AsBool(int index) const;

  // Returns the following based on the type:
  //   - bool: 1 if true, 0 if false.
  //   - int: The int value.
  //   - float: The truncated value of the float, clamped to the integer range.
  //   - string: The int value of the string, or 0 if the string is not a valid
  //     integer.
  //   - map / array: Zero
  int64_t AsInt() const;
  int64_t AsInt(std::string_view key) const;
  int64_t AsInt(int index) const;

  // Returns the following based on the type:
  //   - bool: 1.0 if true, 0.0 if false.
  //   - int: The float value of the int. This is approximate for values beyond
  //   the precision of the float.
  //   - float: The float value.
  //   - string: The float value of the string, or 0.0 if the string is not a
  //     valid float.
  //   - map / array: Zero
  double AsFloat() const;
  double AsFloat(std::string_view key) const;
  double AsFloat(int index) const;

  // Returns the following based on the type:
  //   - bool: "true" or "false".
  //   - int: The string value of the int.
  //   - float: The string value of the float.
  //   - string: The string value.
  //   - map / array: Empty string.
  std::string AsString() const;
  std::string AsString(std::string_view key) const;
  std::string AsString(int index) const;

  // Returns the following based on the type:
  //   - bool: Map of any empty key to the bool value.
  //   - int: Map of any empty key to the int value.
  //   - float: Map of any empty key to the float value.
  //   - string: Map of any empty key to the string value.
  //   - map: The map value.
  //   - array: Map of the string value of each index to the value at that
  //     index. Note that the resulting keys cannot be sorted by string value to
  //     get the same order as was in the array (as "10" sorts before "2", for
  //     instance).
  MapValue AsMap() const;

  // Returns the following based on the type:
  //   - bool: Array containing the bool value.
  //   - int: Array containing the int value.
  //   - float: Array containing the float value.
  //   - string: Array containing the string value.
  //   - map: Array containing the value at each key, ordered by key.
  //   - array: The array value.
  ArrayValue AsArray() const;

 private:
  explicit Config(Value value) : value_(std::move(value)) {}

  Value value_;
};

}  // namespace gb

#endif  // GB_CONFIG_CONFIG_H_
