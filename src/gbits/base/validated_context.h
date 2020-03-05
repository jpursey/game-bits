#ifndef GBITS_BASE_VALIDATED_CONTEXT_H_
#define GBITS_BASE_VALIDATED_CONTEXT_H_

#include <any>
#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "gbits/base/context.h"

namespace gb {

// ContextConstraint describes a possible value of a ValidatedContext or
// ContextContract, and how it should be handled.
//
// ContextConstraint instances are designed to be created as constants and then
// used in the construction of a ValidatedContext or type instantiation of a
// ContextContract.
struct ContextConstraint {
  // Presence determines when a value may or must exist during the lifetime of
  // a ValidatedContext. The same value (type + name) may appear in multiple
  // constraints for a ValidatedContext or ContextContract list as long as they
  // have different compatible Presence values (see below).
  enum Presence {
    kInOptional,  // Value may be in the context at constuction or assignment.
                  // Cannot be used in conjunction with kInRequired.
    kInRequired,  // Value must be in the context at construction or assignment.
                  // Cannot be used in conjunction with kInOptional.
    kOutOptional,  // Value may be added to the context at completion or
                   // destruction. Cannot be used in conjunction with
                   // kOutRequired.
    kOutRequired,  // Value will be added to the context at completion or
                   // destruction. Cannot be used in conjunction with
                   // kOutOptional.
    kScoped,  // Value cannot exist beyond the completion or destruction of the
              // ValidatedContext. The ValidatedContext Complete() function or
              // destructor will automatically clear these.
  };

  // Presence setting for the value (see enum).
  const Presence presence;

  // Type key of the value. This must not be null.
  ContextKey* const type_key;

  // String name for the type. This is used only for debug printing of
  // constraints.
  const std::string type_name;

  // Optional name for the value. If empty, this value is not keyed by name,
  const std::string name;

  // Type used to set the default value. This MUST match the type used for
  // type_key, or the wrong type of value will be set for that key. This must be
  // set if default_value is set.
  ContextType* const any_type = nullptr;

  // If this is set for a kInOptional or kOutOptional constraint, and it is not
  // specified during assignment or completion (resepctively) of the
  // ValidatedContext, then it will be set to this value automatically.
  // kInRequired, kOutRequired, and kScoped values may not have a default value
  // (it will be ignored, if specified).
  const std::any default_value;

  // Helper to display a descriptive string for this constraint for use in
  // logging or debugging.
  std::string ToString() const;
};

// Helper macros to initialize ContextConstraint objects. These are defined
// inline, which means they can be used in header files safely (which is the
// common pattern). When used in a class, be sure to add "static" before the
// macro. See ValidatedContext for example uses.

#define GB_CONTEXT_CONSTRAINT(ConstantName, Presence, Type)         \
  inline const gb::ContextConstraint ConstantName = {               \
      gb::ContextConstraint::Presence, gb::ContextKey::Get<Type>(), \
      gb::ContextTypeName<Type>(nullptr, #Type, true),              \
  }

#define GB_CONTEXT_CONSTRAINT_DEFAULT(ConstantName, Presence, Type, Default) \
  inline const gb::ContextConstraint ConstantName = {                        \
      gb::ContextConstraint::Presence,                                       \
      gb::ContextKey::Get<Type>(),                                           \
      gb::ContextTypeName<Type>(nullptr, #Type, true),                       \
      {},                                                                    \
      gb::ContextType::Get<Type>(),                                          \
      Default,                                                               \
  }

#define GB_CONTEXT_CONSTRAINT_NAMED(ConstantName, Presence, Type, Name) \
  inline const gb::ContextConstraint ConstantName = {                   \
      gb::ContextConstraint::Presence, gb::ContextKey::Get<Type>(),     \
      gb::ContextTypeName<Type>(nullptr, #Type, true), Name,            \
  }

#define GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(ConstantName, Presence, Type, \
                                            Name, Default)                \
  inline const gb::ContextConstraint ConstantName = {                     \
      gb::ContextConstraint::Presence,                                    \
      gb::ContextKey::Get<Type>(),                                        \
      gb::ContextTypeName<Type>(nullptr, #Type, true),                    \
      Name,                                                               \
      gb::ContextType::Get<Type>(),                                       \
      Default,                                                            \
  }

template <const ContextConstraint&... Constraints>
class ContextContract;

// This class defines a validated wrapper around a Context.
//
// As a Context can hold anything, the dynamic nature of the context can hide
// bugs in pre/post conditions of classes that use the context. This class
// addresses these issues by explicitly enforcing the preconditions at
// construction and post-conditions at destruction.
//
// Classes that get a ValidatedContext can only read or write depending based on
// the ContextConstraint parameters defined as part of the ValidatedContext type
// definition.
//
// This class is thread-compatible.
class ValidatedContext final {
 public:
  using ErrorCallback = std::function<void(const std::string& message)>;

  // Default constructor.
  //
  // A default constructed context starts invalid, but will not generate errors
  // on destruction. It can be move-assigned to from either another
  // ValidatedContext or a ContextContract, or initialized with the explicit
  // Initialize() functions.
  ValidatedContext() = default;

  // Construction directly from a Context or ValidatedContext and set of valid
  // context constraints.
  //
  // Validation is performed, and only an underlying context that meets the
  // kInOptional and kInRequired constraints will be accepted. If validation
  // fails, then the ValidatedContext will remain uninitialized, and IsValid()
  // will return false.
  ValidatedContext(Context* context,
                   std::vector<ContextConstraint> constraints) {
    Assign(context, std::move(constraints));
  }
  ValidatedContext(ValidatedContext& other,
                   std::vector<ContextConstraint> constraints) {
    Assign(other, std::move(constraints));
  }
  ValidatedContext(Context&& context,
                   std::vector<ContextConstraint> constraints) {
    Assign(std::move(context), std::move(constraints));
  }
  ValidatedContext(std::unique_ptr<Context> unique_context,
                   std::vector<ContextConstraint> constraints) {
    Assign(std::move(unique_context), std::move(constraints));
  }
  ValidatedContext(std::shared_ptr<Context> shared_context,
                   std::vector<ContextConstraint> constraints) {
    Assign(shared_context, std::move(constraints));
  }

  // Const copy construction is not allowed, as it could allow the underlying
  // const context to be modified.
  ValidatedContext(const ValidatedContext&) = delete;

  // Move construction from a ValidatedContext or ContextContract. Any
  // kOutOptional and kOutRequired ContextConstraint post-conditions will now be
  // enforced by this instance, instead of the moved-from instance, which will
  // become uninitialized (IsValid() will return false).
  ValidatedContext(ValidatedContext&& other) { Assign(std::move(other)); }
  template <const ContextConstraint&... Constraints>
  ValidatedContext(ContextContract<Constraints...>&& contract) {
    Assign(std::move(contract));
  }

  // Const copy assignment is not allowed for ValidatedContext, as it could
  // allow the underlying const context to be modified.
  ValidatedContext& operator=(const ValidatedContext&) = delete;

  // Move assignment from another ValidatedContext or ContextContract.
  //
  // If this context has any existing context and/or constraints, then it
  // will be completed first (as if calling Complete()). This ValidatedContext
  // will now maintain the constraints from for moved-from context or contract.
  //
  // In all cases, the moved-from context or contract will become uninitialized
  // (IsValid() will return false). To avoid unexpected and/or unhandled
  // completion failures, it is recommeneded to call and check Complete()
  // explicitly before move-assignment.
  ValidatedContext& operator=(ValidatedContext&& other) {
    Assign(std::move(other));
    return *this;
  }
  template <const ContextConstraint&... Constraints>
  ValidatedContext& operator=(ContextContract<Constraints...>&& contract) {
    Assign(std::move(contract));
    return *this;
  }

  // The destructor will enforce any ContextConstraint::kOutOptional and
  // ContextConstraint::kOutRequired constraints for this context. Any
  // ContextConstraint::kScoped constraint values will be cleared.
  ~ValidatedContext() { Complete(); }

  // Sets the error callback that will be called if any validation error occurs.
  // If not set, this will LOG(ERROR) in release builds and LOG(FATAL) in debug
  // builds.
  static void SetGlobalErrorCallback(ErrorCallback callback);

  // Assignment from from a Context or ValidatedContext and set of valid context
  // constraints.
  //
  // Validation on the new context and constraints is performed, and only
  // an underlying context that meets the kInOptional and kInRequired
  // constraints will be accepted. If this context is currently valid (has an
  // existing context and constraints), then it will complete the existing
  // constraints before assignment takes place.
  //
  // If Assign fails (returns false) then no completion or assignment is done
  // and or modification to any context is made (unless ownership was
  // transferred). To avoid unexpected and/or unhandled completion failures, it
  // is recommeneded to call and check Complete() explicitly before
  // move-assignment.
  bool Assign(Context* context, std::vector<ContextConstraint> constraints);
  bool Assign(ValidatedContext& context,
              std::vector<ContextConstraint> constraints) {
    if (!Assign(context.context_, std::move(constraints))) {
      return false;
    }
    shared_context_ = context.shared_context_;
    return true;
  }
  bool Assign(Context&& context, std::vector<ContextConstraint> constraints) {
    return Assign(std::make_shared<Context>(std::move(context)),
                  std::move(constraints));
  }
  bool Assign(std::unique_ptr<Context> context,
              std::vector<ContextConstraint> constraints) {
    return Assign(absl::ShareUniquePtr(std::move(context)),
                  std::move(constraints));
  }
  bool Assign(std::shared_ptr<Context> context,
              std::vector<ContextConstraint> constraints) {
    if (!Assign(context.get(), std::move(constraints))) {
      return false;
    }
    shared_context_ = context;
    return true;
  }

  // Move assignment from another ValidatedContext or ContextContract.
  //
  // If this context has any existing context and/or constraints, then it
  // will be completed first (as if calling Complete()). This ValidatedContext
  // will now maintain the constraints from for moved-from context or contract,
  // and this will return true if completion was successful.
  //
  // In all cases, the moved-from context or contract will become uninitialized
  // (IsValid() will return false). To avoid unexpected and/or unhandled
  // completion failures, it is recommeneded to call and check Complete()
  // explicitly before move-assignment.
  bool Assign(ValidatedContext&& context);
  template <const ContextConstraint&... Constraints>
  bool Assign(ContextContract<Constraints...>&& contract);

  // Completes the context, applying all kOutRequired, kOutOptional, and kScoped
  // constraints.
  //
  // On success, the resulting context is reset to an uninitialized state
  // (IsValid() will return false), and this will return true. Otherwise, this
  // returns false, and the underlying context remains unmodified.
  //
  // If the context is already uninitialized (IsValid() returns false),
  // Complete() will trivially succeed and return true.
  bool Complete();

  // Returns true if the ValidatedContext is valid. If this is false, all
  // modification operations will fail, and get operations will behave as though
  // the context is empty.
  bool IsValid() const { return context_ != nullptr; }

  // This will validate that the context will be completed/destructed without
  // any errors.
  bool IsValidToComplete() const { return CanComplete(false); }

  // Returns the context managed by this ValidatedContext.
  //
  // In order to get the best use of the ValidatedContext, it is highly
  // recommended to use the methods directly on ValidateContext instead of
  // calling methods directly on the underlying context. Otherwise, it is easy
  // to defeat the read/write safeguards provided by this class.
  //
  // The returned context must not be destroyed or moved-from as long as any
  // ValidatedContext or ContextContract continues to reference it.
  const Context* GetContext() const { return context_; }
  Context* GetContext() { return context_; }

  // Returns all constraints used to enforce validation by this class.
  const std::vector<ContextConstraint>& GetConstraints() const {
    return constraints_;
  }

  // The following functions mirror the functions in Context, but do additional
  // validation to ensure the operation is allowed. See Context class for full
  // documentation on these methods.
  template <typename Type, class... Args>
  bool SetNew(Args&&... args) {
    if (!CanWriteValue({}, ContextKey::Get<Type>())) {
      return false;
    }
    context_->SetNew<Type, Args...>(std::forward<Args>(args)...);
    return true;
  }
  template <typename Type, class... Args>
  bool SetNamedNew(std::string_view name, Args&&... args) {
    if (!CanWriteValue(name, ContextKey::Get<Type>())) {
      return false;
    }
    context_->SetNamedNew<Type, Args...>(name, std::forward<Args>(args)...);
    return true;
  }
  template <typename Type>
  bool SetOwned(std::unique_ptr<Type> value) {
    if (!CanWriteValue({}, ContextKey::Get<Type>())) {
      return false;
    }
    context_->SetOwned<Type>(std::move(value));
    return true;
  }
  template <typename Type>
  bool SetOwned(std::string_view name, std::unique_ptr<Type> value) {
    if (!CanWriteValue(name, ContextKey::Get<Type>())) {
      return false;
    }
    context_->SetOwned<Type>(name, std::move(value));
    return true;
  }
  template <typename Type>
  bool SetPtr(Type* value) {
    if (!CanWriteValue({}, ContextKey::Get<Type>())) {
      return false;
    }
    context_->SetPtr<Type>({}, value);
    return true;
  }
  template <typename Type>
  bool SetPtr(std::string_view name, Type* value) {
    if (!CanWriteValue(name, ContextKey::Get<Type>())) {
      return false;
    }
    context_->SetPtr<Type>(name, value);
    return true;
  }
  template <typename Type, typename OtherType>
  bool SetValue(OtherType&& value) {
    if (!CanWriteValue({}, ContextKey::Get<Type>())) {
      return false;
    }
    context_->SetValue<Type>(std::forward<OtherType>(value));
    return true;
  }
  template <typename Type, typename OtherType>
  bool SetValue(std::string_view name, OtherType&& value) {
    if (!CanWriteValue(name, ContextKey::Get<Type>())) {
      return false;
    }
    context_->SetValue<Type>(name, std::forward<OtherType>(value));
    return true;
  }
  template <typename Type>
  Type* GetPtr(std::string_view name = {}) const {
    if (!CanReadValue(name, ContextKey::Get<Type>())) {
      return nullptr;
    }
    return context_->GetPtr<Type>(name);
  }
  template <typename Type>
  Type GetValue(std::string_view name = {}) const {
    if (!CanReadValue(name, ContextKey::Get<Type>())) {
      return Type{};
    }
    return context_->GetValue<Type>(name);
  }
  template <typename Type, typename DefaultType>
  Type GetValueOrDefault(DefaultType&& default_value) const {
    if (!CanReadValue({}, ContextKey::Get<Type>())) {
      return std::forward<DefaultType>(default_value);
    }
    return context_->GetValueOrDefault<Type>(
        std::forward<DefaultType>(default_value));
  }
  template <typename Type, typename DefaultType>
  Type GetValueOrDefault(std::string_view name,
                         DefaultType&& default_value) const {
    if (!CanReadValue(name, ContextKey::Get<Type>())) {
      return std::forward<DefaultType>(default_value);
    }
    return context_->GetValueOrDefault<Type>(
        name, std::forward<DefaultType>(default_value));
  }
  template <typename Type>
  bool Exists(std::string_view name = {}) const {
    if (!CanReadValue(name, ContextKey::Get<Type>())) {
      return false;
    }
    return context_->Exists<Type>(name);
  }
  bool Exists(std::string_view name, ContextKey* key) const {
    if (!CanReadValue(name, key)) {
      return false;
    }
    return context_->Exists(name, key);
  }
  bool Exists(ContextKey* key) const {
    if (!CanReadValue({}, key)) {
      return false;
    }
    return context_->Exists(key);
  }

  bool NameExists(std::string_view name) const {
    if (!CanReadValue(name, nullptr)) {
      return false;
    }
    return context_->NameExists(name);
  }
  template <typename Type>
  bool Owned(std::string_view name = {}) const {
    if (!CanReadValue(name, ContextKey::Get<Type>())) {
      return false;
    }
    return context_->Owned<Type>(name);
  }
  template <typename Type>
  std::unique_ptr<Type> Release(std::string_view name = {}) {
    if (!CanWriteValue(name, ContextKey::Get<Type>())) {
      return nullptr;
    }
    return context_->Release<Type>(name);
  }
  template <typename Type>
  bool Clear(std::string_view name = {}) {
    if (!CanWriteValue(name, ContextKey::Get<Type>())) {
      return false;
    }
    context_->Clear<Type>(name);
    return true;
  }
  bool ClearName(std::string_view name) {
    if (!CanWriteValue(name, nullptr)) {
      return false;
    }
    context_->ClearName(name);
    return true;
  }

 private:
  template <const ContextConstraint&... Constraints>
  friend class ContextContract;

  bool CanReadValue(std::string_view name, ContextKey* key) const;
  bool CanWriteValue(std::string_view name, ContextKey* key) const;
  bool CanComplete(bool report_errors) const;
  void ReportError(const std::string& message) const;

  std::shared_ptr<Context> shared_context_;
  Context* context_ = nullptr;
  std::vector<ContextConstraint> constraints_;
};

// This class defines a contract for a ValidateContext as part of the type
// definition.
//
// This allows the specification for contract validation to be an enforced part
// of an APIs contract in the header. This gives a stronger guarantee to the
// caller than just documentation, as the contract is enforced at runtime. A
// ContextContract must be converted to a ValidatedContract before it can be
// used. It is highly recommended that public APIs use a ContextContract instead
// of Context or ValidatedContext directly.

// The GB_CONTEXT_* family of macros provide a more streamlined way to define
// ContextConstraint constants, however their use is simply syntactic sugar and
// is completely optional.
//
// Example with a global function:
//
//   GB_CONTEXT_CONSTRAINT(kSize, kOptional, int);
//
//   void DoSomething(ValidatedContext<kSize> context, ...);
//
// Example as part of a class:
//
//   class Foo {
//    public:
//     static GB_CONTEXT_CONSTRAINT_NAMED(kSize, kRequired, int, "size");
//     using ConstructionContext = ValidatedContext<kSize>;
//
//     Foo(ConstructionContext context, ...);
//
//     ...
//   };
//
// This class is thread-compatible.
template <const ContextConstraint&... Constraints>
class ContextContract final {
 public:
  // Constructs a contract from a raw context. The passed in context must
  // outlive this contract or any resulting ValidateContext that is built from
  // it.
  ContextContract(Context* context) : context_(context, {Constraints...}) {}

  // Constucts a contract from a raw context, taking ownership over the context
  // via move-semantics.
  ContextContract(Context&& context)
      : context_(std::move(context), {Constraints...}) {}

  // Constucts a contract from a raw context, taking ownership over the context.
  ContextContract(std::unique_ptr<Context> context)
      : context_(std::move(context), {Constraints...}) {}

  // Constructs a contract with shared ownership of the context.
  ContextContract(std::shared_ptr<Context> context)
      : context_(context, {Constraints...}) {}

  // Standard copy construction is not allowed, as it could allow the underlying
  // const context to be modified.
  ContextContract(const ContextContract<Constraints...>&) = delete;

  // Construct one contract from another. As long as the underlying context is
  // valid for both contracts, the resulting ValidatedContext will be valid.
  template <const ContextConstraint&... OtherConstraints>
  ContextContract(ContextContract<OtherConstraints...>& other)
      : context_(other.context_.context_, {Constraints...}) {}

  // Construct a contract from a ValidatedContext. As long as the underlying
  // context is valid for this contract as well, the resulting ValidatedContext
  // will be valid.
  ContextContract(ValidatedContext& other)
      : context_(other.context_, {Constraints...}) {}

  // Move construction is supported, but only for the exact same contract.
  // Otherwise, the previous contract may not have its output and scope
  // requirements met.
  ContextContract(ContextContract<Constraints...>&& other) = default;

  // Assignment is not allowed for ContextContract.
  ContextContract<Constraints...>& operator=(
      const ContextContract<Constraints...>&) = delete;
  ContextContract<Constraints...>& operator=(
      ContextContract<Constraints...>&&) = delete;

  // Unless this contract was moved to a ValidatedContract, the contract will be
  // enforced (any optional exports will receive their default values and any
  // required exports that are missing will generate errors).
  ~ContextContract() = default;

  // True if the contract was met and the resulting ValidaredContext will be
  // valid.
  bool IsValid() const { return context_.IsValid(); }

  // Statically returns the constraints that this contract enforces.
  static std::vector<ContextConstraint> GetConstraints() {
    return {Constraints...};
  }

 private:
  friend class ValidatedContext;

  ValidatedContext context_;
};

template <const ContextConstraint&... Constraints>
bool ValidatedContext::Assign(ContextContract<Constraints...>&& contract) {
  return Assign(std::move(contract.context_));
}

}  // namespace gb

#endif  // GBITS_BASE_VALIDATED_CONTEXT_H_
