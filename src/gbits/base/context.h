#ifndef GBITS_BASE_CONTEXT_H_
#define GBITS_BASE_CONTEXT_H_

#include "absl/container/flat_hash_map.h"

#include "gbits/base/context_type.h"

namespace gb {

// This class contains a set of values keyed by type.
//
// Only one value of each type may be stored. Context is a move-only type, and
// so may contain other move-only types.
class Context final {
 public:
  Context() = default;
  Context(const Context&) = delete;
  Context(Context&&) = default;
  Context& operator=(const Context&) = delete;
  Context& operator=(Context&&) = default;
  ~Context() { Reset(); }

  bool IsEmpty() const { return values_.empty(); }

  void Reset();

  template <typename Type, class... Args>
  void SetNew(Args&&... args) {
    SetImpl(ContextType::Get<Type>(), new Type(std::forward<Args>(args)...),
            true);
  }

  template <typename Type>
  void SetOwned(std::unique_ptr<Type> value) {
    SetImpl(ContextType::Get<Type>(), value.release(), true);
  }

  template <typename Type>
  void SetPtr(Type* value) {
    SetImpl(ContextType::Get<Type>(), value.release(), false);
  }

  template <typename Type>
  void SetValue(Type&& value) {
    using StoredType = std::decay_t<Type>;
    SetImpl(ContextType::Get<StoredType>(),
            new StoredType(std::forward<Type>(value)), true);
  }

  template <typename Type>
  Type* Get() const {
    auto it = values_.find(ContextType::Get<Type>());
    return it != values_.end() ? static_cast<Type*>(it->second.value) : nullptr;
  }

  template <typename Type>
  Type GetValue() const {
    Type* value = Get<Type>();
    return value != nullptr ? *value : {};
  }

  template <typename Type>
  bool Exists() const {
    return values_.find(ContextType::Get<Type>()) != values_.end();
  }

  template <typename Type>
  bool Owned() const {
    auto it = values_.find(ContextType::Get<Type>());
    return it != values_.end() && it->second.owned;
  }

  template <typename Type>
  std::unique_ptr<Type> Release() {
    return std::unique_ptr<Type>(ReleaseImpl(ContextType::Get<Type>()));
  }

  template <typename Type>
  void Clear() {
    SetImpl(ContextType::Get<Type>(), nullptr, false);
  }

 private:
  struct Value {
    Value() = default;
    void* value = nullptr;
    bool owned = false;
  };

  void SetImpl(ContextType* type, void* new_value, bool owned);
  void* ReleaseImpl(ContextType* type);

  absl::flat_hash_map<ContextType*, Value> values_;
};

}  // namespace gb

#endif  // GBITS_BASE_CONTEXT_H_