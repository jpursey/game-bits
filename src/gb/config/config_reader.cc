// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/config/config_reader.h"

#include <cmath>

#include "absl/algorithm/container.h"
#include "absl/strings/str_cat.h"

namespace gb {

namespace {

// Returns "1 value" or "3 values".
std::string ValueCount(int count) {
  return absl::StrCat(count, count == 1 ? " value" : " values");
}

}  // namespace

ConfigReader::ConfigReader(const Config* config, std::string_view source)
    : state_(std::make_shared<State>(source)),
      config_(config),
      location_(config != nullptr ? config->GetLocation() : ConfigLocation()) {}

ConfigReader::ConfigReader(std::shared_ptr<State> state, const Config* config,
                           std::string path, ConfigLocation location)
    : state_(std::move(state)),
      config_(config),
      path_(std::move(path)),
      location_(location) {}

absl::Status ConfigReader::GetStatus() const {
  absl::MutexLock lock(&state_->mutex);
  return state_->status;
}

void ConfigReader::Reject(std::string_view message) const {
  absl::MutexLock lock(&state_->mutex);
  if (!state_->status.ok()) {
    return;
  }
  std::string text = state_->source;
  if (location_.IsSet()) {
    absl::StrAppend(&text, "(", location_.line, ":", location_.column, ")");
  }
  if (!text.empty()) {
    text += ": ";
  }
  if (!path_.empty()) {
    absl::StrAppend(&text, path_, ": ");
  }
  absl::StrAppend(&text, message);
  state_->status = absl::InvalidArgumentError(text);
}

ConfigReader ConfigReader::GetChild(const Config* child,
                                    std::string path) const {
  return ConfigReader(state_, child, std::move(path),
                      child != nullptr ? child->GetLocation() : location_);
}

ConfigReader ConfigReader::Get(std::string_view key) const {
  if (config_ != nullptr && !config_->IsMap()) {
    Reject("must be a map");
  }
  return GetChild(
      config_ != nullptr ? config_->Get(key) : nullptr,
      path_.empty() ? std::string(key) : absl::StrCat(path_, ".", key));
}

ConfigReader ConfigReader::Get(int index) const {
  if (config_ != nullptr && !config_->IsArray()) {
    Reject("must be an array");
  }
  return GetChild(config_ != nullptr ? config_->Get(index) : nullptr,
                  absl::StrCat(path_, "[", index, "]"));
}

int ConfigReader::GetArraySize() const {
  return config_ != nullptr ? config_->GetArraySize() : 0;
}

bool ConfigReader::ValidateMap(absl::Span<const std::string_view> keys) const {
  if (config_ == nullptr) {
    return false;
  }
  if (!config_->IsMap()) {
    Reject("must be a map");
    return false;
  }
  const std::string* unknown_key = nullptr;
  for (const auto& [key, value] : *config_->GetMap()) {
    if (absl::c_linear_search(keys, key)) {
      continue;
    }
    if (unknown_key == nullptr || key < *unknown_key) {
      unknown_key = &key;
    }
  }
  if (unknown_key != nullptr) {
    Get(*unknown_key).Reject("unknown key");
    return false;
  }
  return true;
}

bool ConfigReader::ValidateArray(int min_size, int max_size) const {
  DCHECK(min_size >= 0 && min_size <= max_size);
  if (config_ == nullptr) {
    return false;
  }
  if (!config_->IsArray()) {
    Reject("must be an array");
    return false;
  }
  const int size = config_->GetArraySize();
  if (size >= min_size && size <= max_size) {
    return true;
  }
  if (min_size == max_size) {
    Reject(absl::StrCat("must have ", ValueCount(min_size)));
  } else if (max_size == std::numeric_limits<int>::max()) {
    Reject(absl::StrCat("must have at least ", ValueCount(min_size)));
  } else {
    Reject(absl::StrCat("must have ", min_size, " to ", ValueCount(max_size)));
  }
  return false;
}

void ConfigReader::ReadBool(bool* value) const {
  if (config_ == nullptr) {
    return;
  }
  if (!config_->IsBool()) {
    Reject("must be a bool");
    return;
  }
  *value = config_->GetBool();
}

void ConfigReader::ReadString(std::string* value) const {
  if (config_ == nullptr) {
    return;
  }
  if (!config_->IsString()) {
    Reject("must be a string");
    return;
  }
  *value = config_->GetString();
}

bool ConfigReader::DoReadInt(int64_t min, int64_t max, int64_t* value) const {
  if (config_ == nullptr) {
    return false;
  }
  if (!config_->IsInt()) {
    Reject("must be an int");
    return false;
  }
  const int64_t result = config_->GetInt();
  if (result < min || result > max) {
    Reject(absl::StrCat("must be ", min, " to ", max));
    return false;
  }
  *value = result;
  return true;
}

bool ConfigReader::DoReadFloat(double max, double* value) const {
  if (config_ == nullptr) {
    return false;
  }
  double result = 0;
  if (config_->IsFloat()) {
    result = config_->GetFloat();
  } else if (config_->IsInt()) {
    result = static_cast<double>(config_->GetInt());
  } else {
    Reject("must be a float");
    return false;
  }
  if (std::isfinite(result) && std::abs(result) > max) {
    Reject(absl::StrCat("must be -", max, " to ", max));
    return false;
  }
  *value = result;
  return true;
}

void ConfigReader::RejectEnum(absl::Span<const std::string_view> names) const {
  std::string message = "must be ";
  const int count = static_cast<int>(names.size());
  for (int i = 0; i < count; ++i) {
    if (i > 0) {
      message += count == 2 ? " " : ", ";
      if (i == count - 1) {
        message += "or ";
      }
    }
    absl::StrAppend(&message, "\"", names[i], "\"");
  }
  Reject(message);
}

}  // namespace gb
