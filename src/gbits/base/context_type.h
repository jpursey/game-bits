#ifndef GBITS_BASE_CONTEXT_TYPE_H_
#define GBITS_BASE_CONTEXT_TYPE_H_

#include <type_traits>

namespace gb {

class ContextType {
public:
  ContextType(const ContextType&) = delete;
  ContextType& operator=(const ContextType&) = delete;

  template <typename Type>
  static ContextType* Get();

  virtual void Destroy(void* value) = 0;

protected:
  ContextType() = default;
  virtual ~ContextType() = default;

private:
  template <typename Type>
  class Impl;
};

template <typename Type>
class ContextType::Impl : public ContextType {
 public:
  Impl() = default;
  ~Impl() override = default;

  void Destroy(void* value) override { delete static_cast<Type*>(value); }
};

template <typename Type>
ContextType* ContextType::Get() {
  static_assert(std::is_same_v<Type, std::decay_t<Type>>,
                "Invalid type for Context. It is likely const, a function "
                "reference (instead of a pointer), or an array.");
  static Impl<Type> s_context;
  return &s_context;
}

}  // namespace gb

#endif  // GBITS_BASE_CONTEXT_TYPE_H_