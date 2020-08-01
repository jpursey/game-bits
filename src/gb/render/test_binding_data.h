// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_BINDING_DATA_H_
#define GB_RENDER_TEST_BINDING_DATA_H_

#include "absl/types/span.h"
#include "gb/render/binding.h"
#include "gb/render/binding_data.h"

namespace gb {

// Implementation of BindingData for use in tests.
class TestBindingData final : public BindingData {
 public:
  class Value final {
   public:
    Value() = default;
    explicit Value(Binding binding);
    Value(const Value&) = delete;
    Value(Value&& other);
    Value& operator=(const Value&) = delete;
    Value& operator=(Value&& other);
    ~Value();

    const Binding& GetBinding() const { return binding_; }
    TypeKey* GetType() const { return type_; }
    void* GetValue() const { return value_; }
    int GetSize() const { return size_; }

   private:
    Binding binding_;
    TypeKey* type_ = nullptr;
    void* value_ = nullptr;
    int size_ = 0;
  };

  TestBindingData(RenderPipeline* pipeline, BindingSet set,
                  absl::Span<const Binding> bindings);
  ~TestBindingData() override = default;

  absl::Span<const Value> GetValues() const { return values_; }

 protected:
  bool Validate(int index, TypeKey* type) const override;
  void DoSet(int index, const void* value) override;
  void DoGet(int index, void* value) const override;
  void DoGetDependencies(ResourceDependencyList* dependencies) const override;

 private:
  std::vector<Value> values_;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_BINDING_DATA_H_
