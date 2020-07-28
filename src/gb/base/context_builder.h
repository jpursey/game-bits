// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_CONTEXT_BUILDER_H_
#define GB_BASE_CONTEXT_BUILDER_H_

#include "gb/base/context.h"

namespace gb {

// ContextBuilder allows creation of a Context inline, for use with functions
// that take a Context, ValidatedContext, or ContextContract as a parameter.
//
// Example:
//
// auto foo = Foo::Create(gb::ContextBuilder()
//                            .SetValue<std::string>("name", "New Foo")
//                            .SetValue<int>("width", 1024)
//                            .SetValue<int>("height", 768)
//                            .Build());
class ContextBuilder final {
public:
  // Standard constructors and destructors.
  ContextBuilder() = default;
  ContextBuilder(const ContextBuilder&) = delete;
  ContextBuilder(ContextBuilder&&) = default;
  ContextBuilder& operator=(const ContextBuilder&) = delete;
  ContextBuilder& operator=(ContextBuilder&&) = default;
  ~ContextBuilder() = default;

  // Seeds the builder with an initial context.
  ContextBuilder(Context&& context) : context_(std::move(context)) {}

  // Builds a context, moving the internal context to the caller. This
  // effectively resets the internal context to being empty again.
  Context Build() { return std::move(context_); }

  // The following functions mirror the Set* functions in Context. See Context
  // class for full documentation on these methods.
  ContextBuilder& SetParent(WeakPtr<const Context> context) {
    context_.SetParent(context);
    return *this;
  }
  ContextBuilder& SetParent(const ValidatedContext& context) {
    context_.SetParent(context.GetContext());
    return *this;
  }
  template <typename Type, class... Args>
  ContextBuilder& SetNew(Args&&... args) {
    context_.SetNew<Type, Args...>(std::forward<Args>(args)...);
    return *this;
  }
  template <typename Type, class... Args>
  ContextBuilder& SetNamedNew(std::string_view name, Args&&... args) {
    context_.SetNamedNew<Type, Args...>(name, std::forward<Args>(args)...);
    return *this;
  }
  template <typename Type>
  ContextBuilder& SetOwned(std::unique_ptr<Type> value) {
    context_.SetOwned<Type>(std::move(value));
    return *this;
  }
  template <typename Type>
  ContextBuilder& SetOwned(std::string_view name, std::unique_ptr<Type> value) {
    context_.SetOwned<Type>(name, std::move(value));
    return *this;
  }
  template <typename Type>
  ContextBuilder& SetPtr(Type* value) {
    context_.SetPtr<Type>({}, value);
    return *this;
  }
  template <typename Type>
  ContextBuilder& SetPtr(std::string_view name, Type* value) {
    context_.SetPtr<Type>(name, value);
    return *this;
  }
  template <typename Type, typename OtherType>
  ContextBuilder& SetValue(OtherType&& value) {
    context_.SetValue<Type>(std::forward<OtherType>(value));
    return *this;
  }
  template <typename Type, typename OtherType>
  ContextBuilder& SetValue(std::string_view name, OtherType&& value) {
    context_.SetValue<Type>(name, std::forward<OtherType>(value));
    return *this;
  }

private:
  Context context_;
};

}  // namespace gb

#endif  // GB_BASE_CONTEXT_BUILDER_H_ 
