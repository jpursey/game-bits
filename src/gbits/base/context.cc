#include "gbits/base/context.h"

namespace gb {

void Context::SetImpl(ContextType* type, void* new_value, bool owned) {
  auto& stored_value = values_[type];
  if (new_value == nullptr) {
    if (stored_value.owned) {
      type->Destroy(stored_value.value);
    }
    values_.erase(type);
    return;
  }
  if (new_value == stored_value.value) {
    return;
  }
  if (stored_value.owned) {
    type->Destroy(stored_value.value);
  }
  stored_value.value = new_value;
  stored_value.owned = owned;
}

void* Context::ReleaseImpl(ContextType* type) {
  void* released_value = nullptr;
  auto it = values_.find(type);
  if (it != values_.end() && it->second.owned) {
    released_value = it->second.value;
    values_.erase(it);
  }
  return released_value;
}

void Context::Reset() {
  for (auto& value : values_) {
    if (value.second.owned) {
      value.first->Destroy(value.second.value);
    }
  }
  values_.clear();
}

}  // namespace gb
