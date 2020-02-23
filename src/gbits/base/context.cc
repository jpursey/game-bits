#include "gbits/base/context.h"

#include <cstring>

namespace gb {

void Context::SetImpl(std::string_view name, ContextType* type, void* new_value,
                      bool owned) {
  auto key = std::make_tuple(name, type->Key());
  auto it = values_.find(key);
  Value* stored_value = nullptr;
  if (it != values_.end()) {
    stored_value = &it->second;
    if (new_value == nullptr) {
      char* owned_name = stored_value->name;
      if (stored_value->owned) {
        stored_value->type->Destroy(stored_value->value);
      }
      values_.erase(it);
      if (owned_name != nullptr) {
        names_.erase(std::get<0>(key));
        delete[] owned_name;  // Must be after erase.
      }
      return;
    }
    if (new_value == stored_value->value) {
      return;
    }
  } else if (new_value == nullptr) {
    if (!name.empty()) {
      auto name_it = names_.find(name);
      if (name_it != names_.end()) {
        SetImpl(name, name_it->second, nullptr, false);
        return;
      }
    }
    return;
  } else {
    char* owned_name = nullptr;
    if (!name.empty()) {
      auto name_it = names_.find(name);
      if (name_it != names_.end()) {
        SetImpl(name, name_it->second, nullptr, false);
      }
      owned_name = new char[name.size() + 1];
      std::memcpy(owned_name, name.data(), name.size());
      owned_name[name.size()] = 0;
      std::string_view name_key = owned_name;
      key = std::make_tuple(name_key, type->Key());
      names_[name_key] = type;
    }
    stored_value = &values_[key];
    stored_value->name = owned_name;
  }
  if (stored_value->owned) {
    stored_value->type->Destroy(stored_value->value);
  }
  stored_value->type = type;
  stored_value->value = new_value;
  stored_value->owned = owned;
}

void* Context::ReleaseImpl(std::string_view name, ContextType* type) {
  auto key = std::make_pair(name, type->Key());
  void* released_value = nullptr;
  auto it = values_.find(key);
  if (it != values_.end() && it->second.owned) {
    char* owned_name = it->second.name;
    released_value = it->second.value;
    auto name_key = std::get<0>(it->first);
    values_.erase(it);
    if (owned_name != nullptr) {
      names_.erase(name_key);
      delete[] owned_name;  // Must be after erase.
    }
  }
  return released_value;
}

void Context::Reset() {
  std::vector<char*> names;
  names.reserve(values_.size());

  for (auto& value : values_) {
    if (value.second.name != nullptr) {
      names.push_back(value.second.name);
    }
    if (value.second.owned) {
      value.second.type->Destroy(value.second.value);
    }
  }
  values_.clear();
  names_.clear();

  // Must be after clear.
  for (char* name : names) {
    delete[] name;
  }
}

}  // namespace gb
