// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONFIG_CONFIG_READER_H_
#define GB_CONFIG_CONFIG_READER_H_

#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gb/config/config.h"
#include "gb/config/config_types.h"

namespace gb {

// An integer type ConfigReader::ReadInt() can read into: any integral type but
// bool and the character types, which ReadBool() and ReadString() read
// instead. Enums are not integral types; ReadEnum() reads them by name.
template <typename T>
concept ConfigInt =
    std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, char> &&
    !std::same_as<T, char8_t> && !std::same_as<T, char16_t> &&
    !std::same_as<T, char32_t> && !std::same_as<T, wchar_t>;

// The name an enum value has in a config, for ConfigReader::ReadEnum():
//   constexpr gb::ConfigEnumName<DeviceType> kDeviceTypes[] = {
//       {"screen", DeviceType::kScreen}, {"keyboard", DeviceType::kKeyboard}};
template <typename T>
  requires std::is_enum_v<T>
struct ConfigEnumName {
  std::string_view name;
  T value;
};

// Reads a config into a program's own settings, checking each value's type and
// range as it goes:
//   gb::ConfigReader reader(&config, filename);
//   if (reader.ValidateMap(kMachineKeys)) {
//     reader.Get("cores").ReadInt(1, kMaxCores, &settings.cores);
//     reader.Get("name").ReadString(&settings.name);
//   }
//   return reader.GetStatus();
//
// Each reader is a view of one value, with its path from the root
// ("devices[1].columns"). Every reader made from the same root, by copying it
// or by Get(), shares that read's status, which keeps the first error any of
// them finds:
//   hello.ozzy(12:5): devices[1].columns: must be 1 to 132
//
// After an error, reads still read, and later errors are dropped, so a caller
// can read everything and return GetStatus() once at the end.
//
// A missing value (a key not in its map, or an index past the end of its
// array) can still be read from, and leaves every value read from it as it
// was. Its errors give the location of the value it is missing from.
//
// This class is thread-compatible. The status shared by every reader of one
// read is thread-safe, so different readers of it may be used on different
// threads at once.
class ConfigReader final {
 public:
  // Reads `config`, which may be null for a missing config. It must outlive
  // this and every reader made from it. Errors begin with `source` (such as the
  // file the config was read from), if it isn't empty.
  explicit ConfigReader(const Config* config, std::string_view source = {});

  // A copy is the same view of the same read. There is no move, which would
  // leave a reader with no read: moving copies.
  ConfigReader(const ConfigReader&) = default;
  ConfigReader& operator=(const ConfigReader&) = default;
  ~ConfigReader() = default;

  //----------------------------------------------------------------------------
  // Status
  //----------------------------------------------------------------------------

  // Returns OK, or the first error any reader of this read has found, as an
  // InvalidArgument error.
  absl::Status GetStatus() const;

  // Records an error at this value, if the read has none yet:
  // "<source>(<line>:<column>): <path>: <message>". Parts that aren't known are
  // left out: the source if it is empty, the location if the value has none,
  // and the path at the root.
  void Reject(std::string_view message) const;

  //----------------------------------------------------------------------------
  // Value
  //----------------------------------------------------------------------------

  // Returns the value, or null if it is missing.
  const Config* GetConfig() const { return config_; }

  // Returns true if the value is missing.
  bool IsMissing() const { return config_ == nullptr; }

  // Returns the path from the root to this value ("devices[1].columns"), or
  // empty for the root.
  std::string_view GetPath() const { return path_; }

  //----------------------------------------------------------------------------
  // Maps and arrays
  //----------------------------------------------------------------------------

  // Returns the value at `key`, or a missing value if there is no such key.
  // Rejects this value if it is present and isn't a map.
  ConfigReader Get(std::string_view key) const;

  // Returns the value at `index`, or a missing value if it is out of range.
  // Rejects this value if it is present and isn't an array.
  ConfigReader Get(int index) const;

  // Returns the number of values in this array, or zero if it is missing or
  // isn't an array.
  int GetArraySize() const;

  // Returns true if this is a map with no keys but `keys` (it need not have
  // all of them). Rejects it if it isn't a map, or at its first unknown key in
  // sorted order (maps are unordered) with "unknown key". Returns false, with
  // no error, if it is missing.
  bool ValidateMap(absl::Span<const std::string_view> keys) const;

  // Returns true if this is an array of `min_size` to `max_size` values.
  // Rejects it if not. Returns false, with no error, if it is missing.
  bool ValidateArray(int min_size = 0,
                     int max_size = std::numeric_limits<int>::max()) const;

  //----------------------------------------------------------------------------
  // Reads
  //
  // Each reads this value into `value`, rejecting it if it is the wrong type or
  // out of range, in which case `value` is left as it was. A missing value also
  // leaves `value` as it was, so it keeps the caller's default.
  //----------------------------------------------------------------------------

  void ReadBool(bool* value) const;
  void ReadString(std::string* value) const;

  // Reads an int, which must fit in T. A Config int is an int64_t, so a
  // uint64_t only reads up to INT64_MAX. Floats are rejected.
  template <ConfigInt T>
  void ReadInt(T* value) const;

  // Reads an int from `min` to `max`, which must both fit in T.
  template <ConfigInt T>
  void ReadInt(int64_t min, int64_t max, T* value) const;

  // Reads a float, or an int as its float value. A value too big for T is
  // rejected, rather than read as infinity.
  template <std::floating_point T>
  void ReadFloat(T* value) const;

  // Reads a string naming one of `names`, which must not be empty. Names match
  // exactly, case included. Anything else is rejected, with the names listed:
  // `must be "screen", "keyboard", or "teletype"`. T must be given explicitly
  // when `names` is an array: ReadEnum<DeviceType>(kDeviceTypes, &type).
  template <typename T>
    requires std::is_enum_v<T>
  void ReadEnum(absl::Span<const ConfigEnumName<T>> names, T* value) const;

 private:
  // The state shared by every reader of one read.
  struct State {
    explicit State(std::string_view source) : source(source) {}

    const std::string source;
    absl::Mutex mutex;
    absl::Status status ABSL_GUARDED_BY(mutex);
  };

  ConfigReader(std::shared_ptr<State> state, const Config* config,
               std::string path, ConfigLocation location);

  // Returns a reader for `child` (null if it is missing) at `path` below this
  // one. A missing child gives this value's location.
  ConfigReader GetChild(const Config* child, std::string path) const;

  // Reads an int from `min` to `max`. Returns false if the value is missing or
  // was rejected.
  bool DoReadInt(int64_t min, int64_t max, int64_t* value) const;

  // Reads a float of at most `max` in size. Returns false if the value is
  // missing or was rejected.
  bool DoReadFloat(double max, double* value) const;

  // Rejects the value as not one of `names`.
  void RejectEnum(absl::Span<const std::string_view> names) const;

  std::shared_ptr<State> state_;
  const Config* config_;
  std::string path_;

  // The location errors give: the value's own, or for a missing value, that
  // of the value it is missing from.
  ConfigLocation location_;
};

//==============================================================================
// Template implementation
//==============================================================================

template <ConfigInt T>
void ConfigReader::ReadInt(T* value) const {
  int64_t max = std::numeric_limits<int64_t>::max();
  if constexpr (std::in_range<int64_t>(std::numeric_limits<T>::max())) {
    max = std::numeric_limits<T>::max();
  }
  ReadInt(std::numeric_limits<T>::min(), max, value);
}

template <ConfigInt T>
void ConfigReader::ReadInt(int64_t min, int64_t max, T* value) const {
  DCHECK(std::in_range<T>(min) && std::in_range<T>(max) && min <= max);
  int64_t result = 0;
  if (DoReadInt(min, max, &result)) {
    *value = static_cast<T>(result);
  }
}

template <std::floating_point T>
void ConfigReader::ReadFloat(T* value) const {
  double max = std::numeric_limits<double>::max();
  if constexpr (std::numeric_limits<T>::max() <
                std::numeric_limits<double>::max()) {
    max = std::numeric_limits<T>::max();
  }
  double result = 0;
  if (DoReadFloat(max, &result)) {
    *value = static_cast<T>(result);
  }
}

template <typename T>
  requires std::is_enum_v<T>
void ConfigReader::ReadEnum(absl::Span<const ConfigEnumName<T>> names,
                            T* value) const {
  DCHECK(!names.empty());
  if (config_ == nullptr) {
    return;
  }
  if (config_->IsString()) {
    const std::string_view text = config_->GetString();
    for (const ConfigEnumName<T>& name : names) {
      if (name.name == text) {
        *value = name.value;
        return;
      }
    }
  }
  std::vector<std::string_view> name_list;
  name_list.reserve(names.size());
  for (const ConfigEnumName<T>& name : names) {
    name_list.push_back(name.name);
  }
  RejectEnum(name_list);
}

}  // namespace gb

#endif  // GB_CONFIG_CONFIG_READER_H_
