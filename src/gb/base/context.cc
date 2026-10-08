// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/context.h"

#include <cstring>
#include <utility>

namespace gb {

Context::Context(Context&& other) : WeakScope<Context>(this) {
  *this = std::move(other);
}

Context& Context::operator=(Context&& other) {
  if (&other == this) {
    return *this;
  }
  {
    absl::WriterMutexLock other_lock(&other.mutex_);
    absl::WriterMutexLock this_lock(&mutex_);
    parent_ = std::exchange(other.parent_, nullptr);
    std::swap(values_, other.values_);
    std::swap(names_, other.names_);
  }

  // `other` now holds this context's old values, which are destroyed outside
  // of the locks.
  other.Reset();
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
      if (stored_value->owned) {
        delete_type = stored_value->type;
        delete_value = stored_value->value;
      }
      if (stored_value->name != nullptr) {
        names_.erase(name);
      }
      values_.erase(it);

      mutex_.WriterUnlock();
      if (delete_value != nullptr) {
        delete_type->Destroy(delete_value);
      }
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
    std::unique_ptr<char[]> owned_name;
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
      owned_name = std::make_unique_for_overwrite<char[]>(name.size());
      std::memcpy(owned_name.get(), name.data(), name.size());
      const std::string_view name_key(owned_name.get(), name.size());
      key = std::make_tuple(name_key, type->Key());
      names_[name_key] = type;
    }
    stored_value = &values_[key];
    stored_value->name = std::move(owned_name);
  }

  // Setting the value it already holds only changes its ownership.
  if (stored_value->owned && stored_value->value != new_value) {
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
  auto it = values_.find(std::make_tuple(name, type->Key()));
  if (it == values_.end() || !it->second.owned) {
    return nullptr;
  }
  void* released_value = it->second.value;
  if (it->second.name != nullptr) {
    names_.erase(name);
  }
  values_.erase(it);
  return released_value;
}

void Context::Reset() {
  Values old_values;
  {
    absl::WriterMutexLock lock(&mutex_);
    names_.clear();
    old_values = std::exchange(values_, {});
  }
  for (auto& [key, value] : old_values) {
    if (value.owned) {
      value.type->Destroy(value.value);
    }
  }
}

}  // namespace gb
