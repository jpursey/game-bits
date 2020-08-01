// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_binding_data.h"

namespace gb {

TestBindingData::Value::Value(Binding binding) : binding_(binding) {
  switch (binding_.binding_type) {
    case BindingType::kNone:
      break;
    case BindingType::kConstants:
      type_ = binding.constants_type->GetType();
      size_ = binding.constants_type->GetSize();
      value_ = std::calloc(size_, 1);
      break;
    case BindingType::kTexture:
      type_ = TypeKey::Get<Texture*>();
      size_ = sizeof(Texture*);
      value_ = std::calloc(size_, 1);
      break;
    default:
      LOG(FATAL) << "Unhandled binding type in TestBindingData";
  }
}

TestBindingData::Value::Value(Value&& other)
    : binding_(std::exchange(other.binding_, Binding())),
      type_(std::exchange(other.type_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      value_(std::exchange(other.value_, nullptr)) {}

TestBindingData::Value& TestBindingData::Value::operator=(Value&& other) {
  if (&other != this) {
    std::free(value_);
    binding_ = std::exchange(other.binding_, Binding());
    type_ = std::exchange(other.type_, nullptr);
    size_ = std::exchange(other.size_, 0);
    value_ = std::exchange(other.value_, nullptr);
  }
  return *this;
}

TestBindingData::Value::~Value() { std::free(value_); }

TestBindingData::TestBindingData(RenderPipeline* pipeline, BindingSet set,
                                 absl::Span<const Binding> bindings)
    : BindingData(pipeline, set) {
  for (const Binding& binding : bindings) {
    if (binding.set != set) {
      continue;
    }
    if (binding.index >= static_cast<int>(values_.size())) {
      values_.resize(binding.index + 1);
    }
    values_[binding.index] = Value(binding);
  }
}

bool TestBindingData::Validate(int index, TypeKey* type) const {
  return index >= 0 && index < static_cast<int>(values_.size()) &&
         values_[index].GetType() == type;
}

void TestBindingData::DoSet(int index, const void* value) {
  std::memcpy(values_[index].GetValue(), value, values_[index].GetSize());
}

void TestBindingData::DoGet(int index, void* value) const {
  std::memcpy(value, values_[index].GetValue(), values_[index].GetSize());
}

void TestBindingData::DoGetDependencies(
    ResourceDependencyList* dependencies) const {
  static TypeKey* const texture_type = TypeKey::Get<Texture*>();
  for (const auto& value : values_) {
    if (value.GetType() == texture_type) {
      Texture* texture = *static_cast<Texture**>(value.GetValue());
      if (texture != nullptr) {
        dependencies->push_back(texture);
      }
    }
  }
}

}  // namespace gb
