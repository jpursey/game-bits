// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/context.h"

#include <cstring>

namespace gb {

Context::Context(Context&& other) : WeakScope<Context>(this) {
  *this = std::move(other);
}

Context& Context::operator=(Context&& other) {
  absl::WriterMutexLock other_lock(&other.mutex_);
  absl::WriterMutexLock this_lock(&mutex_);
  parent_ = std::move(other.parent_);
  values_ = std::move(other.values_);
  names_ = std::move(other.names_);
  return *this;
}

void Context::SetImpl(std::string_view name, TypeInfo* type, void* new_value,
                      bool owned) {
  TypeInfo* delete_type = nullptr;
  void* delete_value = nullptr;
  auto key = std::make_tuple(name, type->Key());
  Value* stored_value = nullptr;

  mutex_.WriterLock();
  auto it = values_.find(key);
  if (it != values_.end()) {
    // The value already exists. If we are replacing with null, then we erase
    // (and delete if owned) the existing value.
    stored_value = &it->second;
    if (new_value == nullptr) {
      char* owned_name = stored_value->name;
      if (stored_value->owned) {
        delete_type = stored_value->type;
        delete_value = stored_value->value;
      }
      values_.erase(it);
      if (owned_name != nullptr) {
        names_.erase(std::get<0>(key));
        delete[] owned_name;  // Must be after erase.
      }

      mutex_.WriterUnlock();
      if (delete_value != nullptr) {
        delete_type->Destroy(delete_value);
      }
      return;
    }

    // If the new and old value are identical, there is nothing to do.
    if (new_value == stored_value->value) {
      mutex_.WriterUnlock();
      return;
    }
  } else if (new_value == nullptr) {
    // We are erasing a value that does not exist. However, if a name was
    // specified, this must clear any other value of the same name.
    if (!name.empty()) {
      auto name_it = names_.find(name);
      if (name_it != names_.end()) {
        mutex_.WriterUnlock();
        SetImpl(name, name_it->second, nullptr, false);
        return;
      }
    }
    mutex_.WriterUnlock();
    return;
  } else {
    // We are adding a new value!
    char* owned_name = nullptr;
    if (!name.empty()) {
      // A name was specified, so we must first clear any existing value of the
      // same name.
      auto name_it = names_.find(name);
      if (name_it != names_.end()) {
        mutex_.WriterUnlock();
        SetImpl(name, name_it->second, nullptr, false);
        SetImpl(name, type, new_value, owned);
        return;
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
    delete_type = stored_value->type;
    delete_value = stored_value->value;
  }
  stored_value->type = type;
  stored_value->value = new_value;
  stored_value->owned = owned;

  mutex_.WriterUnlock();
  if (delete_value != nullptr) {
    delete_type->Destroy(delete_value);
  }
}

void* Context::ReleaseImpl(std::string_view name, TypeInfo* type) {
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
  Values old_values;
  {
    absl::WriterMutexLock lock(&mutex_);
    old_values = std::move(values_);
    names_.clear();
  }

  std::vector<char*> names;
  names.reserve(old_values.size());

  for (auto& value : old_values) {
    if (value.second.name != nullptr) {
      names.push_back(value.second.name);
    }
    if (value.second.owned) {
      value.second.type->Destroy(value.second.value);
    }
  }

  // Must be after clear.
  for (char* name : names) {
    delete[] name;
  }
}

}  // namespace gb
