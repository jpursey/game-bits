#include "gb/base/validated_context.h"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::Test;

// Equality operator for EXPECT_EQ support in tests.
AssertionResult Equal(const std::vector<ContextConstraint>& a,
                      const std::vector<ContextConstraint>& b) {
  if (a.size() != b.size()) {
    return AssertionFailure()
           << "a.size (" << a.size() << ") != b.size (" << b.size() << ")";
  }
  for (int i = 0; i < static_cast<int>(a.size()); ++i) {
    if (a[i].presence != b[i].presence || a[i].type_key != b[i].type_key ||
        a[i].name != b[i].name) {
      return AssertionFailure()
             << "a[" << i << "] (" << a[i].ToString() << ") != b[" << i << "] ("
             << b[i].ToString() << ")";
    }
  }
  return AssertionSuccess();
}

struct Counts {
  Counts() = default;
  int destruct = 0;
  int construct = 0;
  int copy_construct = 0;
  int move_construct = 0;
  int copy_assign = 0;
  int move_assign = 0;
};

class Item {
 public:
  Item(Counts* counts) : counts(counts) { ++counts->construct; }
  Item(const Item& other) : counts(other.counts) { ++counts->copy_construct; }
  Item(Item&& other) : counts(other.counts) { ++counts->move_construct; }
  Item& operator=(const Item& other) {
    ++counts->copy_assign;
    return *this;
  }
  Item& operator=(Item&& other) {
    ++counts->move_assign;
    return *this;
  }
  ~Item() { ++counts->destruct; }

 private:
  Counts* counts;
};

constexpr char* kNameWidth = "Width";
constexpr char* kNameHeight = "Height";
constexpr char* kNameScore = "Score";
constexpr char* kNameValue = "Value";

constexpr int kDefaultInWidth = 100;
constexpr int kDefaultInHeight = 200;
constexpr int kDefaultOutWidth = 300;
constexpr int kDefaultOutHeight = 400;
constexpr int kDefaultInValue = 1000;
constexpr int kDefaultOutValue = 2000;

GB_CONTEXT_CONSTRAINT_NAMED(kInRequiredWidth, kInRequired, int, kNameWidth);
GB_CONTEXT_CONSTRAINT_NAMED(kInRequiredHeight, kInRequired, int, kNameHeight);
GB_CONTEXT_CONSTRAINT_NAMED(kInRequiredNamedValue, kInRequired, int,
                            kNameValue);
GB_CONTEXT_CONSTRAINT(kInRequiredItem, kInRequired, Item);
GB_CONTEXT_CONSTRAINT(kInRequiredValue, kInRequired, int);

GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kInOptionalWidth, kInOptional, int,
                                    kNameWidth, kDefaultInWidth);
GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kInOptionalHeight, kInOptional, int,
                                    kNameHeight, kDefaultInHeight);
GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kInOptionalNamedValue, kInOptional, int,
                                    kNameValue, kDefaultInValue);
GB_CONTEXT_CONSTRAINT(kInOptionalItem, kInOptional, Item);
GB_CONTEXT_CONSTRAINT_DEFAULT(kInOptionalValue, kInOptional, int,
                              kDefaultInValue);

GB_CONTEXT_CONSTRAINT_NAMED(kOutRequiredWidth, kOutRequired, int, kNameWidth);
GB_CONTEXT_CONSTRAINT_NAMED(kOutRequiredHeight, kOutRequired, int, kNameHeight);
GB_CONTEXT_CONSTRAINT_NAMED(kOutRequiredNamedValue, kOutRequired, int,
                            kNameValue);
GB_CONTEXT_CONSTRAINT(kOutRequiredItem, kOutRequired, Item);
GB_CONTEXT_CONSTRAINT(kOutRequiredValue, kOutRequired, int);

GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kOutOptionalWidth, kOutOptional, int,
                                    kNameWidth, kDefaultOutWidth);
GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kOutOptionalHeight, kOutOptional, int,
                                    kNameHeight, kDefaultOutHeight);
GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kOutOptionalNamedValue, kOutOptional, int,
                                    kNameValue, kDefaultOutValue);
GB_CONTEXT_CONSTRAINT(kOutOptionalItem, kOutOptional, Item);
GB_CONTEXT_CONSTRAINT_DEFAULT(kOutOptionalValue, kOutOptional, int,
                              kDefaultOutValue);

GB_CONTEXT_CONSTRAINT_NAMED(kScopedScore, kScoped, int, kNameScore);
GB_CONTEXT_CONSTRAINT(kScopedItem, kScoped, Item);
GB_CONTEXT_CONSTRAINT(kScopedValue, kScoped, int);
GB_CONTEXT_CONSTRAINT_NAMED(kScopedNamedValue, kScoped, int, kNameValue);

class ContextTest : public Test {
 public:
  ContextTest() {
    ValidatedContext::SetGlobalErrorCallback(
        [this](const std::string& message) { ++error_count_; });
  }
  virtual ~ContextTest() { ValidatedContext::SetGlobalErrorCallback(nullptr); }

  int GetErrorCount() const { return error_count_; }

 private:
  int error_count_ = 0;
};

//------------------------------------------------------------------------------
// Test types

enum TestSubType {
  kWithContext,
  kWithValidatedContext,
  kWithMoveValidatedContext,
  kWithMoveContract,
};

template <TestSubType SubType>
struct ContextContractConstructTestType {
  template <const ContextConstraint&... Constraints>
  static bool Assign(Context* context) {
    if (SubType == kWithContext) {
      ContextContract<Constraints...> contract(context);
      return contract.IsValid();
    } else if (SubType == kWithValidatedContext) {
      ValidatedContext validated_context(context, {});
      ContextContract<Constraints...> contract(validated_context);
      return contract.IsValid();
    } else if (SubType == kWithMoveContract) {
      ContextContract<Constraints...> in_contract(context);
      ContextContract<Constraints...> contract(std::move(in_contract));
      EXPECT_FALSE(in_contract.IsValid());
      return contract.IsValid();
    }
    LOG(FATAL) << "Invalid type parameter";
    return false;
  }
};

template <TestSubType SubType>
struct ValidatedContextConstructTestType {
  template <const ContextConstraint&... Constraints>
  static bool Assign(Context* context) {
    if (SubType == kWithContext) {
      ValidatedContext validated_context(context, {Constraints...});
      return validated_context.IsValid();
    } else if (SubType == kWithValidatedContext) {
      ValidatedContext in_validated_context(context, {});
      ValidatedContext validated_context(in_validated_context,
                                         {Constraints...});
      return validated_context.IsValid();
    } else if (SubType == kWithMoveValidatedContext) {
      ValidatedContext in_validated_context(context, {Constraints...});
      ValidatedContext validated_context(std::move(in_validated_context));
      EXPECT_FALSE(in_validated_context.IsValid());
      return validated_context.IsValid();
    } else if (SubType == kWithMoveContract) {
      ContextContract<Constraints...> contract(context);
      ValidatedContext validated_context(std::move(contract));
      EXPECT_FALSE(contract.IsValid());
      return validated_context.IsValid();
    }
    LOG(FATAL) << "Invalid type parameter";
    return false;
  }
};

template <TestSubType SubType>
struct ValidatedContextAssignMethodTestType {
  template <const ContextConstraint&... Constraints>
  static bool Assign(Context* context) {
    if (SubType == kWithContext) {
      ValidatedContext validated_context;
      bool result = validated_context.Assign(context, {Constraints...});
      EXPECT_EQ(result, validated_context.IsValid());
      return result;
    } else if (SubType == kWithValidatedContext) {
      ValidatedContext in_validated_context(context, {});
      ValidatedContext validated_context;
      bool result =
          validated_context.Assign(in_validated_context, {Constraints...});
      EXPECT_EQ(result, validated_context.IsValid());
      return result;
    } else if (SubType == kWithMoveValidatedContext) {
      ValidatedContext in_validated_context(context, {Constraints...});
      bool input_valid = in_validated_context.IsValid();
      ValidatedContext validated_context;
      bool result = validated_context.Assign(std::move(in_validated_context));
      EXPECT_EQ(input_valid, validated_context.IsValid());
      EXPECT_FALSE(in_validated_context.IsValid());
      return result && input_valid;
    } else if (SubType == kWithMoveContract) {
      ContextContract<Constraints...> contract(context);
      bool input_valid = contract.IsValid();
      ValidatedContext validated_context;
      bool result = validated_context.Assign(std::move(contract));
      EXPECT_EQ(input_valid, validated_context.IsValid());
      EXPECT_FALSE(contract.IsValid());
      return result && input_valid;
    }
    LOG(FATAL) << "Invalid type parameter";
    return false;
  }

  static bool Complete(Context* context,
                       ValidatedContext* out_validated_context) {
    if (SubType == kWithContext) {
      Context* const old_context = out_validated_context->GetContext();
      const std::vector<ContextConstraint> old_constraints =
          out_validated_context->GetConstraints();
      bool result = out_validated_context->Assign(context, {});
      if (!result) {
        EXPECT_EQ(old_context, out_validated_context->GetContext());
        EXPECT_TRUE(
            Equal(old_constraints, out_validated_context->GetConstraints()));
      }
      return result;
    } else if (SubType == kWithValidatedContext) {
      Context* const old_context = out_validated_context->GetContext();
      const std::vector<ContextConstraint> old_constraints =
          out_validated_context->GetConstraints();
      ValidatedContext in_validated_context(context, {});
      bool result = out_validated_context->Assign(in_validated_context, {});
      if (!result) {
        EXPECT_EQ(old_context, out_validated_context->GetContext());
        EXPECT_TRUE(
            Equal(old_constraints, out_validated_context->GetConstraints()));
      }
      return result;
    } else if (SubType == kWithMoveValidatedContext) {
      ValidatedContext in_validated_context(context, {});
      Context* const in_context = in_validated_context.GetContext();
      const std::vector<ContextConstraint> in_constraints =
          in_validated_context.GetConstraints();
      bool result =
          out_validated_context->Assign(std::move(in_validated_context));
      EXPECT_TRUE(out_validated_context->IsValid());
      EXPECT_EQ(in_context, out_validated_context->GetContext());
      EXPECT_TRUE(
          Equal(in_constraints, out_validated_context->GetConstraints()));
      EXPECT_FALSE(in_validated_context.IsValid());
      return result;
    } else if (SubType == kWithMoveContract) {
      ContextContract<> contract(context);
      bool result = out_validated_context->Assign(std::move(contract));
      EXPECT_FALSE(contract.IsValid());
      EXPECT_TRUE(out_validated_context->IsValid());
      EXPECT_EQ(context, out_validated_context->GetContext());
      EXPECT_TRUE(Equal(out_validated_context->GetConstraints(), {}));
      return result;
    } else {
      LOG(FATAL) << "Invalid type parameter";
    }
    return false;
  }
};

template <TestSubType SubType>
struct ValidatedContextAssignOperatorTestType {
  template <const ContextConstraint&... Constraints>
  static bool Assign(Context* context) {
    if (SubType == kWithMoveValidatedContext) {
      ValidatedContext in_validated_context(context, {Constraints...});
      ValidatedContext validated_context;
      validated_context = std::move(in_validated_context);
      EXPECT_FALSE(in_validated_context.IsValid());
      return validated_context.IsValid();
    } else if (SubType == kWithMoveContract) {
      ContextContract<Constraints...> contract(context);
      ValidatedContext validated_context;
      validated_context = std::move(contract);
      EXPECT_FALSE(contract.IsValid());
      return validated_context.IsValid();
    }
    LOG(FATAL) << "Invalid type parameter";
    return false;
  }

  static bool Complete(Context* context,
                       ValidatedContext* out_validated_context) {
    if (SubType == kWithMoveValidatedContext) {
      ValidatedContext in_validated_context(context, {});
      Context* const in_context = in_validated_context.GetContext();
      const std::vector<ContextConstraint> in_constraints =
          in_validated_context.GetConstraints();
      bool result = out_validated_context->IsValidToComplete();
      *out_validated_context = std::move(in_validated_context);
      EXPECT_FALSE(in_validated_context.IsValid());
      EXPECT_TRUE(out_validated_context->IsValid());
      EXPECT_EQ(in_context, out_validated_context->GetContext());
      EXPECT_TRUE(
          Equal(in_constraints, out_validated_context->GetConstraints()));
      return result;
    } else if (SubType == kWithMoveContract) {
      ContextContract<> contract(context);
      bool result = out_validated_context->IsValidToComplete();
      *out_validated_context = std::move(contract);
      EXPECT_FALSE(contract.IsValid());
      EXPECT_TRUE(out_validated_context->IsValid());
      EXPECT_EQ(context, out_validated_context->GetContext());
      EXPECT_TRUE(Equal(out_validated_context->GetConstraints(), {}));
      return result;
    }
    LOG(FATAL) << "Invalid type parameter";
    return false;
  }
};

struct ValidatedContextCompleteMethodTestType {
  static bool Complete(Context* context,
                       ValidatedContext* out_validated_context) {
    bool result = out_validated_context->Complete();
    EXPECT_NE(result, out_validated_context->IsValid());
    return result;
  }
};

struct ValidatedContextDestructorTestType {
  static bool Complete(Context* context,
                       ValidatedContext* out_validated_context) {
    bool result = out_validated_context->IsValidToComplete();
    ValidatedContext validated_context(std::move(*out_validated_context));
    return result;
  }
};

//------------------------------------------------------------------------------
// ConstraintMacroTest

constexpr int kTestConstraintValue = 10;
constexpr char* kTestConstraintName = "name";

GB_CONTEXT_CONSTRAINT(kTestConstraint, kInRequired, int);
GB_CONTEXT_CONSTRAINT_DEFAULT(kTestConstraintDefault, kInRequired, int,
                              kTestConstraintValue);
GB_CONTEXT_CONSTRAINT_NAMED(kTestConstraintNamed, kInRequired, int,
                            kTestConstraintName);
GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kTestConstraintNamedDefault, kInRequired,
                                    int, kTestConstraintName,
                                    kTestConstraintValue);

TEST(ConstraintMacroTest, Constraint) {
  EXPECT_EQ(kTestConstraint.presence, ContextConstraint::kInRequired);
  EXPECT_EQ(kTestConstraint.type_key, TypeKey::Get<int>());
  EXPECT_EQ(kTestConstraint.type_name, "int");
  EXPECT_EQ(kTestConstraint.name, "");
  EXPECT_EQ(kTestConstraint.any_type, nullptr);
  EXPECT_FALSE(kTestConstraint.default_value.has_value());
}

TEST(ConstraintMacroTest, ConstraintDefault) {
  EXPECT_EQ(kTestConstraintDefault.presence, ContextConstraint::kInRequired);
  EXPECT_EQ(kTestConstraintDefault.type_key, TypeKey::Get<int>());
  EXPECT_EQ(kTestConstraintDefault.type_name, "int");
  EXPECT_EQ(kTestConstraintDefault.name, "");
  EXPECT_EQ(kTestConstraintDefault.any_type, TypeInfo::Get<int>());
  ASSERT_TRUE(kTestConstraintDefault.default_value.has_value());
}

TEST(ConstraintMacroTest, ConstraintNamed) {
  EXPECT_EQ(kTestConstraintNamed.presence, ContextConstraint::kInRequired);
  EXPECT_EQ(kTestConstraintNamed.type_key, TypeKey::Get<int>());
  EXPECT_EQ(kTestConstraintNamed.type_name, "int");
  EXPECT_EQ(kTestConstraintNamed.name, kTestConstraintName);
  EXPECT_EQ(kTestConstraintNamed.any_type, nullptr);
  EXPECT_FALSE(kTestConstraintNamed.default_value.has_value());
}

TEST(ConstraintMacroTest, ConstraintNamedDefault) {
  EXPECT_EQ(kTestConstraintNamedDefault.presence,
            ContextConstraint::kInRequired);
  EXPECT_EQ(kTestConstraintNamedDefault.type_key, TypeKey::Get<int>());
  EXPECT_EQ(kTestConstraintNamedDefault.type_name, "int");
  EXPECT_EQ(kTestConstraintNamedDefault.name, kTestConstraintName);
  EXPECT_EQ(kTestConstraintNamedDefault.any_type, TypeInfo::Get<int>());
  ASSERT_TRUE(kTestConstraintNamedDefault.default_value.has_value());
}

//------------------------------------------------------------------------------
// ValidatedContextTest

class ValidatedContextTest : public ContextTest {};

TEST_F(ValidatedContextTest, DefaultConstructionIsInvalid) {
  ValidatedContext validated_context;
  EXPECT_FALSE(validated_context.IsValid());
  EXPECT_EQ(validated_context.GetContext(), nullptr);
  EXPECT_TRUE(validated_context.GetConstraints().empty());
}

TEST_F(ValidatedContextTest, InvalidContextIsValidToComplete) {
  ValidatedContext validated_context;
  EXPECT_TRUE(validated_context.IsValidToComplete());
}

TEST_F(ValidatedContextTest, ConstraintsAreMoved) {
  Context context;
  std::vector<ContextConstraint> constraints = {kOutOptionalWidth,
                                                kOutOptionalHeight};
  ValidatedContext validated_context(&context, std::move(constraints));
  EXPECT_EQ(validated_context.GetContext(), &context);
  EXPECT_TRUE(Equal(validated_context.GetConstraints(),
                    {kOutOptionalWidth, kOutOptionalHeight}));
  EXPECT_TRUE(constraints.empty());
}

//------------------------------------------------------------------------------
// AssignContextTest

template <typename TestType>
class AssignContextTest : public ContextTest {};

using AssignContextTestTypes = ::testing::Types<
    ContextContractConstructTestType<kWithContext>,
    ContextContractConstructTestType<kWithValidatedContext>,
    ContextContractConstructTestType<kWithMoveContract>,
    ValidatedContextConstructTestType<kWithContext>,
    ValidatedContextConstructTestType<kWithValidatedContext>,
    ValidatedContextConstructTestType<kWithMoveValidatedContext>,
    ValidatedContextConstructTestType<kWithMoveContract>,
    ValidatedContextAssignMethodTestType<kWithContext>,
    ValidatedContextAssignMethodTestType<kWithValidatedContext>,
    ValidatedContextAssignMethodTestType<kWithMoveValidatedContext>,
    ValidatedContextAssignMethodTestType<kWithMoveContract>,
    ValidatedContextAssignOperatorTestType<kWithMoveValidatedContext>,
    ValidatedContextAssignOperatorTestType<kWithMoveContract>>;
TYPED_TEST_SUITE(AssignContextTest, AssignContextTestTypes);

TYPED_TEST(AssignContextTest, EmptyContextValidWithNoValues) {
  Context context;
  EXPECT_TRUE(TypeParam::Assign(&context));
}

TYPED_TEST(AssignContextTest, EmptyContextValidWithOptionalValues) {
  Context context;
  EXPECT_TRUE(
      (TypeParam::Assign<kInOptionalWidth, kInOptionalHeight, kInOptionalItem>(
          &context)));
  EXPECT_TRUE(context.Exists<int>(kNameWidth));
  EXPECT_TRUE(context.Exists<int>(kNameHeight));
  EXPECT_EQ(context.GetValue<int>(kNameWidth), kDefaultInWidth);
  EXPECT_EQ(context.GetValue<int>(kNameHeight), kDefaultInHeight);
}

TYPED_TEST(AssignContextTest, EmptyContextInvalidWithRequiredValue) {
  Context context;
  EXPECT_FALSE((TypeParam::Assign<kInRequiredItem>(&context)));
  EXPECT_FALSE(context.Exists<Item>());
}

TYPED_TEST(AssignContextTest, EmptyContextInvalidWithRequiredNamedValue) {
  Context context;
  EXPECT_FALSE((TypeParam::Assign<kInRequiredWidth>(&context)));
  EXPECT_FALSE(context.NameExists(kNameWidth));
}

TYPED_TEST(AssignContextTest, ExtraValuesInContextAreValid) {
  Context context;
  context.SetValue<std::string>("This is a string!");
  context.SetValue<double>(kNameWidth, 10.0);
  EXPECT_TRUE(TypeParam::Assign(&context));
  EXPECT_EQ(context.GetValue<std::string>(), "This is a string!");
  EXPECT_EQ(context.GetValue<double>(kNameWidth), 10.0);
}

TYPED_TEST(AssignContextTest, ContextValidWithRequiredValue) {
  Counts counts;
  Context context;
  context.SetValue<Item>(&counts);
  EXPECT_TRUE((TypeParam::Assign<kInRequiredItem>(&context)));
}

TYPED_TEST(AssignContextTest, ContextValidWithRequiredNamedValues) {
  Context context;
  context.SetValue<int>(kNameWidth, 10);
  context.SetValue<int>(kNameHeight, 20);
  EXPECT_TRUE(
      (TypeParam::Assign<kInRequiredWidth, kInRequiredHeight>(&context)));
}

TYPED_TEST(AssignContextTest, ContextValidWithOptionalValue) {
  Counts counts;
  Context context;
  context.SetValue<Item>(&counts);
  EXPECT_TRUE((TypeParam::Assign<kInOptionalItem>(&context)));
}

TYPED_TEST(AssignContextTest, ContextValidWithOptionalNamedValues) {
  Context context;
  context.SetValue<int>(kNameWidth, 10);
  context.SetValue<int>(kNameHeight, 20);
  EXPECT_TRUE(
      (TypeParam::Assign<kInOptionalWidth, kInOptionalHeight, kInOptionalItem>(
          &context)));
  EXPECT_EQ(context.GetValue<int>(kNameWidth), 10);
  EXPECT_EQ(context.GetValue<int>(kNameHeight), 20);
}

TYPED_TEST(AssignContextTest, ContextInvalidWithOptionalNamedValueOfWrongType) {
  Context context;
  context.SetValue<double>(kNameWidth, 10.0);
  context.SetValue<double>(kNameHeight, 20.0);
  EXPECT_FALSE(
      (TypeParam::Assign<kInOptionalWidth, kInOptionalHeight>(&context)));
  EXPECT_EQ(context.GetValue<double>(kNameWidth), 10.0);
  EXPECT_EQ(context.GetValue<double>(kNameHeight), 20.0);
}

TYPED_TEST(AssignContextTest, OnlyInputValuesAreValidated) {
  Counts counts;
  Context context;
  context.SetValue<double>(kNameWidth, 10.0);
  context.SetValue<double>(kNameHeight, 20.0);
  context.SetValue<double>(kNameScore, 30.0);
  EXPECT_TRUE(
      (TypeParam::Assign<kOutOptionalWidth, kOutOptionalHeight, kScopedScore>(
          &context)));
  EXPECT_EQ(context.GetValue<double>(kNameWidth), 10.0);
  EXPECT_EQ(context.GetValue<double>(kNameHeight), 20.0);
  EXPECT_EQ(context.GetValue<double>(kNameScore), 30.0);
}

TYPED_TEST(AssignContextTest, OptionalOutputValuesAreInitialized) {
  Context context;
  EXPECT_TRUE(
      (TypeParam::Assign<kOutOptionalWidth, kOutOptionalHeight>(&context)));
  EXPECT_TRUE(context.Exists<int>(kNameWidth));
  EXPECT_TRUE(context.Exists<int>(kNameHeight));
  EXPECT_EQ(context.GetValue<int>(kNameWidth), kDefaultOutWidth);
  EXPECT_EQ(context.GetValue<int>(kNameHeight), kDefaultOutHeight);
}

TYPED_TEST(AssignContextTest, ScopedValuesAreDeleted) {
  Counts counts;
  Context context;
  context.SetValue<int>(kNameScore, 42);
  context.SetValue<Item>(&counts);
  EXPECT_TRUE((TypeParam::Assign<kScopedItem, kScopedScore>(&context)));
  EXPECT_FALSE(context.Exists<int>(kNameScore));
  EXPECT_FALSE(context.Exists<Item>());
  EXPECT_EQ(counts.destruct, 1);
}

//------------------------------------------------------------------------------
// CompleteContextTest

template <typename TestType>
class CompleteContextTest : public ContextTest {};

using CompleteContextTestTypes = ::testing::Types<
    ValidatedContextAssignMethodTestType<kWithContext>,
    ValidatedContextAssignMethodTestType<kWithValidatedContext>,
    ValidatedContextAssignMethodTestType<kWithMoveValidatedContext>,
    ValidatedContextAssignMethodTestType<kWithMoveContract>,
    ValidatedContextAssignOperatorTestType<kWithMoveValidatedContext>,
    ValidatedContextAssignOperatorTestType<kWithMoveContract>,
    ValidatedContextCompleteMethodTestType, ValidatedContextDestructorTestType>;
TYPED_TEST_SUITE(CompleteContextTest, CompleteContextTestTypes);

TYPED_TEST(CompleteContextTest, MissingRequiredOutputValue) {
  Context context;
  Context new_context;
  ValidatedContext validated_context(&context, {kOutRequiredItem});
  EXPECT_FALSE(validated_context.IsValidToComplete());
  EXPECT_FALSE(TypeParam::Complete(&new_context, &validated_context));
  EXPECT_EQ(GetErrorCount(), 1);
}

TYPED_TEST(CompleteContextTest, MissingNamedRequiredOutputValue) {
  Context context;
  Context new_context;
  ValidatedContext validated_context(&context, {kOutRequiredWidth});
  EXPECT_FALSE(validated_context.IsValidToComplete());
  EXPECT_FALSE(TypeParam::Complete(&new_context, &validated_context));
  EXPECT_EQ(GetErrorCount(), 1);
}

TYPED_TEST(CompleteContextTest, InvalidNamedRequiredOutputValueType) {
  Context context;
  Context new_context;
  context.SetValue<double>(kNameWidth, 10.0);
  ValidatedContext validated_context(&context, {kOutRequiredWidth});
  EXPECT_FALSE(validated_context.IsValidToComplete());
  EXPECT_FALSE(TypeParam::Complete(&new_context, &validated_context));
  EXPECT_EQ(GetErrorCount(), 1);
}

TYPED_TEST(CompleteContextTest, ValidRequiredOutputValues) {
  Counts counts;
  Context context;
  Context new_context;
  context.SetValue<Item>(&counts);
  context.SetValue<int>(kNameWidth, 10);
  context.SetValue<int>(kNameHeight, 20);
  ValidatedContext validated_context(
      &context, {kOutRequiredItem, kOutRequiredWidth, kOutRequiredHeight});
  EXPECT_TRUE(validated_context.IsValidToComplete());
  EXPECT_TRUE(TypeParam::Complete(&new_context, &validated_context));
  EXPECT_EQ(GetErrorCount(), 0);
}

TYPED_TEST(CompleteContextTest, InvalidNamedOptionalOutputValueType) {
  Context context;
  Context new_context;
  context.SetValue<double>(kNameWidth, 10.0);
  ValidatedContext validated_context(&context, {kOutOptionalWidth});
  EXPECT_FALSE(validated_context.IsValidToComplete());
  EXPECT_FALSE(TypeParam::Complete(&new_context, &validated_context));
  EXPECT_EQ(GetErrorCount(), 1);
}

TYPED_TEST(CompleteContextTest, OptionalOutputValuesAreInitialized) {
  Context context;
  Context new_context;
  ValidatedContext validated_context(
      &context, {kOutOptionalItem, kOutOptionalWidth, kOutOptionalHeight});
  EXPECT_TRUE(validated_context.IsValidToComplete());
  EXPECT_TRUE(TypeParam::Complete(&new_context, &validated_context));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_FALSE(context.Exists<Item>());
  EXPECT_EQ(context.GetValue<int>(kNameWidth), kDefaultOutWidth);
  EXPECT_EQ(context.GetValue<int>(kNameHeight), kDefaultOutHeight);
}

TYPED_TEST(CompleteContextTest, OptionalOutputValuesAreNotOverwritten) {
  Counts counts;
  Item item(&counts);
  Context context;
  Context new_context;
  context.SetPtr<Item>(&item);
  context.SetValue<int>(kNameWidth, 10);
  context.SetValue<int>(kNameHeight, 20);
  ValidatedContext validated_context(
      &context, {kOutOptionalItem, kOutOptionalWidth, kOutOptionalHeight});
  EXPECT_TRUE(validated_context.IsValidToComplete());
  EXPECT_TRUE(TypeParam::Complete(&new_context, &validated_context));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_EQ(context.GetPtr<Item>(), &item);
  EXPECT_EQ(context.GetValue<int>(kNameWidth), 10);
  EXPECT_EQ(context.GetValue<int>(kNameHeight), 20);
}

TYPED_TEST(CompleteContextTest, ScopedValuesAreDeleted) {
  Counts counts;
  Context context;
  Context new_context;
  context.SetValue<Item>(&counts);
  context.SetValue<int>(kNameScore, 10);
  ValidatedContext validated_context(&context, {kScopedItem, kScopedScore});
  EXPECT_TRUE(validated_context.IsValidToComplete());
  EXPECT_TRUE(TypeParam::Complete(&new_context, &validated_context));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_FALSE(context.Exists<Item>());
  EXPECT_FALSE(context.NameExists(kNameScore));
}

//------------------------------------------------------------------------------
// Read/Write Tests

class ContextConstraintTest
    : public ContextTest,
      public testing::WithParamInterface<std::vector<ContextConstraint>> {
 public:
  static inline constexpr int kInitialValue = -1;
  static inline constexpr int kInitialNamedValue = -2;
  static inline constexpr int kNewValue = 10;

  ContextConstraintTest() {
    context.SetValue<int>(kInitialValue);
    context.SetValue<int>(kNameValue, kInitialNamedValue);
  }

 protected:
  int value = kNewValue;
  Context context;
};

class WriteFailsTest : public ContextConstraintTest {};
class WriteNamedFailsTest : public ContextConstraintTest {};
class WriteSucceedsTest : public ContextConstraintTest {};
class WriteNamedSucceedsTest : public ContextConstraintTest {};
class ReadFailsTest : public ContextConstraintTest {};
class ReadNamedFailsTest : public ContextConstraintTest {};
class ReadSucceedsTest : public ContextConstraintTest {};
class ReadNamedSucceedsTest : public ContextConstraintTest {};

TEST_P(WriteFailsTest, SetNew) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.SetNew<int>(value));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(), kInitialValue);
}

TEST_P(WriteFailsTest, SetOwned) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.SetOwned<int>(std::make_unique<int>(value)));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(), kInitialValue);
}

TEST_P(WriteFailsTest, SetPtr) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.SetPtr<int>(&value));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(), kInitialValue);
}

TEST_P(WriteFailsTest, SetValue) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.SetValue<int>(value));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(), kInitialValue);
}

TEST_P(WriteFailsTest, Release) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.Release<int>(), nullptr);
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(), kInitialValue);
}

TEST_P(WriteFailsTest, Clear) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.Clear<int>());
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(), kInitialValue);
}

TEST_P(WriteNamedFailsTest, SetNamedNew) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.SetNamedNew<int>(kNameValue, value));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kInitialNamedValue);
}

TEST_P(WriteNamedFailsTest, SetOwned) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.SetOwned<int>(kNameValue,
                                               std::make_unique<int>(value)));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kInitialNamedValue);
}

TEST_P(WriteNamedFailsTest, SetPtr) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.SetPtr<int>(kNameValue, &value));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kInitialNamedValue);
}

TEST_P(WriteNamedFailsTest, SetValue) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.SetValue<int>(kNameValue, value));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kInitialNamedValue);
}

TEST_P(WriteNamedFailsTest, Release) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.Release<int>(kNameValue), nullptr);
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kInitialNamedValue);
}

TEST_P(WriteNamedFailsTest, Clear) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.Clear<int>(kNameValue));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kInitialNamedValue);
}

TEST_P(WriteNamedFailsTest, ClearName) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.ClearName(kNameValue));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kInitialNamedValue);
}

TEST_P(WriteSucceedsTest, SetNew) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.SetNew<int>(value));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_EQ(context.GetValue<int>(), kNewValue);
}

TEST_P(WriteSucceedsTest, SetOwned) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.SetOwned<int>(std::make_unique<int>(value)));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_EQ(context.GetValue<int>(), kNewValue);
}

TEST_P(WriteSucceedsTest, SetPtr) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.SetPtr<int>(&value));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_EQ(context.GetValue<int>(), kNewValue);
}

TEST_P(WriteSucceedsTest, SetValue) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.SetValue<int>(value));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_EQ(context.GetValue<int>(), kNewValue);
}

TEST_P(WriteSucceedsTest, Release) {
  ValidatedContext validated_context(&context, GetParam());
  auto result = validated_context.Release<int>();
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, kInitialValue);
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_FALSE(context.Exists<int>());
}

TEST_P(WriteSucceedsTest, Clear) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.Clear<int>());
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_FALSE(context.Exists<int>());
}

TEST_P(WriteNamedSucceedsTest, SetNamedNew) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.SetNamedNew<int>(kNameValue, value));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kNewValue);
}

TEST_P(WriteNamedSucceedsTest, SetOwned) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.SetOwned<int>(kNameValue,
                                              std::make_unique<int>(value)));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kNewValue);
}

TEST_P(WriteNamedSucceedsTest, SetPtr) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.SetPtr<int>(kNameValue, &value));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kNewValue);
}

TEST_P(WriteNamedSucceedsTest, SetValue) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.SetValue<int>(kNameValue, value));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_EQ(context.GetValue<int>(kNameValue), kNewValue);
}

TEST_P(WriteNamedSucceedsTest, Release) {
  ValidatedContext validated_context(&context, GetParam());
  auto result = validated_context.Release<int>(kNameValue);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, kInitialNamedValue);
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_FALSE(context.Exists<int>(kNameValue));
}

TEST_P(WriteNamedSucceedsTest, Clear) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.Clear<int>(kNameValue));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_FALSE(context.Exists<int>(kNameValue));
}

TEST_P(WriteNamedSucceedsTest, ClearName) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.ClearName(kNameValue));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_FALSE(context.NameExists(kNameValue));
}

TEST_P(ReadFailsTest, GetPtr) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.GetPtr<int>(), nullptr);
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadFailsTest, GetValue) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.GetValue<int>(), 0);
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadFailsTest, GetValueOrDefault) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.GetValueOrDefault<int>(kNewValue), kNewValue);
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadFailsTest, Exists) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.Exists<int>());
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadFailsTest, ExistsWithContextType) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.Exists(TypeKey::Get<int>()));
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadFailsTest, Owned) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.Owned<int>());
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadNamedFailsTest, GetPtr) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.GetPtr<int>(kNameValue), nullptr);
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadNamedFailsTest, GetValue) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.GetValue<int>(kNameValue), 0);
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadNamedFailsTest, GetValueOrDefault) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.GetValueOrDefault<int>(kNameValue, kNewValue),
            kNewValue);
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadNamedFailsTest, Exists) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.Exists<int>(kNameValue));
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadNamedFailsTest, ExistsWithContextType) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.Exists(kNameValue, TypeKey::Get<int>()));
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadNamedFailsTest, Owned) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_FALSE(validated_context.Owned<int>(kNameValue));
  EXPECT_EQ(GetErrorCount(), 1);
}

TEST_P(ReadSucceedsTest, GetPtr) {
  ValidatedContext validated_context(&context, GetParam());
  auto* result = validated_context.GetPtr<int>();
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, kInitialValue);
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadSucceedsTest, GetValue) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.GetValue<int>(), kInitialValue);
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadSucceedsTest, GetValueOrDefault) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.GetValueOrDefault<int>(kNewValue), kInitialValue);
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadSucceedsTest, Exists) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.Exists<int>());
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadSucceedsTest, ExistsWithContextType) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.Exists(TypeKey::Get<int>()));
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadSucceedsTest, Owned) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.Owned<int>());
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadNamedSucceedsTest, GetPtr) {
  ValidatedContext validated_context(&context, GetParam());
  auto* result = validated_context.GetPtr<int>(kNameValue);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, kInitialNamedValue);
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadNamedSucceedsTest, GetValue) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.GetValue<int>(kNameValue), kInitialNamedValue);
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadNamedSucceedsTest, GetValueOrDefault) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_EQ(validated_context.GetValueOrDefault<int>(kNameValue, kNewValue),
            kInitialNamedValue);
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadNamedSucceedsTest, Exists) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.Exists<int>(kNameValue));
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadNamedSucceedsTest, ExistsWithContextType) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.Exists(kNameValue, TypeKey::Get<int>()));
  EXPECT_EQ(GetErrorCount(), 0);
}

TEST_P(ReadNamedSucceedsTest, Owned) {
  ValidatedContext validated_context(&context, GetParam());
  EXPECT_TRUE(validated_context.Owned<int>(kNameValue));
  EXPECT_EQ(GetErrorCount(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    ValidatedContext, WriteFailsTest,
    ::testing::Values(std::vector<ContextConstraint>({}),
                      std::vector<ContextConstraint>({kInRequiredValue}),
                      std::vector<ContextConstraint>({kInOptionalValue})));
INSTANTIATE_TEST_SUITE_P(
    ValidatedContext, WriteNamedFailsTest,
    ::testing::Values(std::vector<ContextConstraint>({}),
                      std::vector<ContextConstraint>({kInRequiredNamedValue}),
                      std::vector<ContextConstraint>({kInOptionalNamedValue})));
INSTANTIATE_TEST_SUITE_P(
    ValidatedContext, WriteSucceedsTest,
    ::testing::Values(
        std::vector<ContextConstraint>({kOutRequiredValue}),
        std::vector<ContextConstraint>({kOutOptionalValue}),
        std::vector<ContextConstraint>({kInRequiredValue, kOutRequiredValue}),
        std::vector<ContextConstraint>({kInOptionalValue, kOutOptionalValue})));
INSTANTIATE_TEST_SUITE_P(
    ValidatedContext, WriteNamedSucceedsTest,
    ::testing::Values(std::vector<ContextConstraint>({kOutRequiredNamedValue}),
                      std::vector<ContextConstraint>({kOutOptionalNamedValue}),
                      std::vector<ContextConstraint>({kInRequiredNamedValue,
                                                      kOutRequiredNamedValue}),
                      std::vector<ContextConstraint>(
                          {kInOptionalNamedValue, kOutOptionalNamedValue})));

INSTANTIATE_TEST_SUITE_P(
    ValidatedContext, ReadFailsTest,
    ::testing::Values(std::vector<ContextConstraint>({}),
                      std::vector<ContextConstraint>({kInRequiredNamedValue}),
                      std::vector<ContextConstraint>({kInOptionalNamedValue})));
INSTANTIATE_TEST_SUITE_P(
    ValidatedContext, ReadNamedFailsTest,
    ::testing::Values(std::vector<ContextConstraint>({}),
                      std::vector<ContextConstraint>({kInRequiredValue}),
                      std::vector<ContextConstraint>({kInOptionalValue})));
INSTANTIATE_TEST_SUITE_P(
    ValidatedContext, ReadSucceedsTest,
    ::testing::Values(std::vector<ContextConstraint>({kInRequiredValue}),
                      std::vector<ContextConstraint>({kInOptionalValue}),
                      std::vector<ContextConstraint>({kOutRequiredValue}),
                      std::vector<ContextConstraint>({kOutOptionalValue}),
                      std::vector<ContextConstraint>({kScopedValue})));
INSTANTIATE_TEST_SUITE_P(
    ValidatedContext, ReadNamedSucceedsTest,
    ::testing::Values(std::vector<ContextConstraint>({kInRequiredNamedValue}),
                      std::vector<ContextConstraint>({kInOptionalNamedValue}),
                      std::vector<ContextConstraint>({kOutRequiredNamedValue}),
                      std::vector<ContextConstraint>({kOutOptionalNamedValue}),
                      std::vector<ContextConstraint>({kScopedNamedValue})));

//------------------------------------------------------------------------------
// ContextOwnershipTest

class ContextOwnershipTest : public ContextTest {};

TEST_F(ContextOwnershipTest, ConstructContractMoveContextSuccess) {
  Counts counts;
  Context context;
  context.SetValue<Item>(&counts);
  Counts init_counts = counts;
  ContextContract<kInOptionalItem> contract(std::move(context));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_TRUE(contract.IsValid());
  EXPECT_TRUE(context.Empty());
  ValidatedContext validated_context(std::move(contract));
  EXPECT_TRUE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructContractMoveContextFailure) {
  Counts counts;
  Context context;
  context.SetValue<Item>(&counts);
  Counts init_counts = counts;
  ContextContract<kInOptionalItem, kInRequiredValue> contract(
      std::move(context));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_FALSE(contract.IsValid());
  EXPECT_TRUE(context.Empty());
  ValidatedContext validated_context(std::move(contract));
  EXPECT_FALSE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct + 1);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructContractUniqueContextSuccess) {
  Counts counts;
  auto context = std::make_unique<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  ContextContract<kInOptionalItem> contract(std::move(context));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_TRUE(contract.IsValid());
  EXPECT_EQ(context, nullptr);
  ValidatedContext validated_context(std::move(contract));
  EXPECT_TRUE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructContractUniqueContextFailure) {
  Counts counts;
  auto context = std::make_unique<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  ContextContract<kInOptionalItem, kInRequiredValue> contract(
      std::move(context));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_FALSE(contract.IsValid());
  EXPECT_EQ(context, nullptr);
  ValidatedContext validated_context(std::move(contract));
  EXPECT_FALSE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct + 1);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructContractSharedContextSuccess) {
  Counts counts;
  auto context = std::make_shared<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  ContextContract<kInOptionalItem> contract(context);
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_TRUE(contract.IsValid());
  EXPECT_TRUE(context->Exists<Item>());
  ValidatedContext validated_context(std::move(contract));
  EXPECT_TRUE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructContractSharedContextFailure) {
  Counts counts;
  auto context = std::make_shared<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  ContextContract<kInOptionalItem, kInRequiredValue> contract(context);
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_FALSE(contract.IsValid());
  EXPECT_TRUE(context->Exists<Item>());
  ValidatedContext validated_context(std::move(contract));
  EXPECT_FALSE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructValidatedContextMoveContextSuccess) {
  Counts counts;
  Context context;
  context.SetValue<Item>(&counts);
  Counts init_counts = counts;
  ValidatedContext validated_context(std::move(context), {kInOptionalItem});
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_TRUE(validated_context.IsValid());
  EXPECT_TRUE(context.Empty());
  EXPECT_TRUE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructValidatedContextMoveContextFailure) {
  Counts counts;
  Context context;
  context.SetValue<Item>(&counts);
  Counts init_counts = counts;
  ValidatedContext validated_context(std::move(context),
                                     {kInOptionalItem, kInRequiredValue});
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_FALSE(validated_context.IsValid());
  EXPECT_TRUE(context.Empty());
  EXPECT_FALSE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct + 1);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructValidatedContextUniqueContextSuccess) {
  Counts counts;
  auto context = std::make_unique<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  ValidatedContext validated_context(std::move(context), {kInOptionalItem});
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_TRUE(validated_context.IsValid());
  EXPECT_EQ(context, nullptr);
  EXPECT_TRUE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructValidatedContextUniqueContextFailure) {
  Counts counts;
  auto context = std::make_unique<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  ValidatedContext validated_context(std::move(context),
                                     {kInOptionalItem, kInRequiredValue});
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_FALSE(validated_context.IsValid());
  EXPECT_EQ(context, nullptr);
  EXPECT_FALSE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct + 1);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructValidatedContextSharedContextSuccess) {
  Counts counts;
  auto context = std::make_shared<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  ValidatedContext validated_context(context, {kInOptionalItem});
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_TRUE(validated_context.IsValid());
  EXPECT_TRUE(context->Exists<Item>());
  EXPECT_TRUE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, ConstructValidatedContextSharedContextFailure) {
  Counts counts;
  auto context = std::make_shared<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  ValidatedContext validated_context(context,
                                     {kInOptionalItem, kInRequiredValue});
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_FALSE(validated_context.IsValid());
  EXPECT_TRUE(context->Exists<Item>());
  EXPECT_FALSE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, AssignValidatedContextMoveContextSuccess) {
  Counts counts;
  Context context;
  context.SetValue<Item>(&counts);
  Counts init_counts = counts;
  Context original_context;
  original_context.SetValue<int>(kDefaultInValue);
  ValidatedContext validated_context(&original_context,
                                     {kInOptionalValue, kOutOptionalWidth});
  EXPECT_TRUE(validated_context.Assign(std::move(context), {kInOptionalItem}));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_NE(validated_context.GetContext(), &original_context);
  EXPECT_EQ(original_context.GetValue<int>(kNameWidth), kDefaultOutWidth);
  EXPECT_FALSE(validated_context.Exists<int>());
  EXPECT_FALSE(validated_context.Exists<int>(kNameWidth));
  EXPECT_TRUE(context.Empty());
  EXPECT_TRUE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, AssignValidatedContextMoveContextFailure) {
  Counts counts;
  Context context;
  context.SetValue<Item>(&counts);
  Counts init_counts = counts;
  Context original_context;
  original_context.SetValue<int>(kDefaultInValue);
  ValidatedContext validated_context(&original_context,
                                     {kInOptionalValue, kOutOptionalWidth});
  EXPECT_FALSE(validated_context.Assign(std::move(context),
                                        {kInOptionalItem, kInRequiredValue}));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(validated_context.GetContext(), &original_context);
  EXPECT_FALSE(original_context.NameExists(kNameWidth));
  EXPECT_EQ(validated_context.GetValue<int>(), kDefaultInValue);
  EXPECT_TRUE(context.Empty());
  EXPECT_FALSE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct + 1);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, AssignValidatedContextUniqueContextSuccess) {
  Counts counts;
  auto context = std::make_unique<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  Context original_context;
  original_context.SetValue<int>(kDefaultInValue);
  ValidatedContext validated_context(&original_context,
                                     {kInOptionalValue, kOutOptionalWidth});
  EXPECT_TRUE(validated_context.Assign(std::move(context), {kInOptionalItem}));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_NE(validated_context.GetContext(), &original_context);
  EXPECT_EQ(original_context.GetValue<int>(kNameWidth), kDefaultOutWidth);
  EXPECT_FALSE(validated_context.Exists<int>());
  EXPECT_FALSE(validated_context.Exists<int>(kNameWidth));
  EXPECT_EQ(context, nullptr);
  EXPECT_TRUE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, AssignValidatedContextUniqueContextFailure) {
  Counts counts;
  auto context = std::make_unique<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  Context original_context;
  original_context.SetValue<int>(kDefaultInValue);
  ValidatedContext validated_context(&original_context,
                                     {kInOptionalValue, kOutOptionalWidth});
  EXPECT_FALSE(validated_context.Assign(std::move(context),
                                        {kInOptionalItem, kInRequiredValue}));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(validated_context.GetContext(), &original_context);
  EXPECT_FALSE(original_context.NameExists(kNameWidth));
  EXPECT_EQ(validated_context.GetValue<int>(), kDefaultInValue);
  EXPECT_EQ(context, nullptr);
  EXPECT_FALSE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct + 1);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, AssignValidatedContextSharedContextSuccess) {
  Counts counts;
  auto context = std::make_shared<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  Context original_context;
  original_context.SetValue<int>(kDefaultInValue);
  ValidatedContext validated_context(&original_context,
                                     {kInOptionalValue, kOutOptionalWidth});
  EXPECT_TRUE(validated_context.Assign(context, {kInOptionalItem}));
  EXPECT_EQ(GetErrorCount(), 0);
  EXPECT_NE(validated_context.GetContext(), &original_context);
  EXPECT_EQ(original_context.GetValue<int>(kNameWidth), kDefaultOutWidth);
  EXPECT_FALSE(validated_context.Exists<int>());
  EXPECT_FALSE(validated_context.Exists<int>(kNameWidth));
  EXPECT_TRUE(context->Exists<Item>());
  EXPECT_TRUE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, AssignValidatedContextSharedContextFailure) {
  Counts counts;
  auto context = std::make_shared<Context>();
  context->SetValue<Item>(&counts);
  Counts init_counts = counts;
  Context original_context;
  original_context.SetValue<int>(kDefaultInValue);
  ValidatedContext validated_context(&original_context,
                                     {kInOptionalValue, kOutOptionalWidth});
  EXPECT_FALSE(
      validated_context.Assign(context, {kInOptionalItem, kInRequiredValue}));
  EXPECT_EQ(GetErrorCount(), 1);
  EXPECT_EQ(validated_context.GetContext(), &original_context);
  EXPECT_FALSE(original_context.NameExists(kNameWidth));
  EXPECT_EQ(validated_context.GetValue<int>(), kDefaultInValue);
  EXPECT_TRUE(context->Exists<Item>());
  EXPECT_FALSE(validated_context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST_F(ContextOwnershipTest, AssignTakesSharedOwnership) {
  auto context = std::make_shared<Context>();
  auto context_ptr = context.get();
  context->SetValue<int>(kDefaultInValue);
  std::vector<ContextConstraint> constraints = {kInRequiredValue};

  auto validated_context_a =
      std::make_unique<ValidatedContext>(context, constraints);
  context = nullptr;
  EXPECT_EQ(validated_context_a->GetContext(), context_ptr);
  EXPECT_EQ(validated_context_a->GetValue<int>(), kDefaultInValue);

  auto validated_context_b =
      std::make_unique<ValidatedContext>(*validated_context_a, constraints);
  validated_context_a = nullptr;
  EXPECT_EQ(validated_context_b->GetContext(), context_ptr);
  EXPECT_EQ(validated_context_b->GetValue<int>(), kDefaultInValue);

  auto validated_context_c = std::make_unique<ValidatedContext>();
  EXPECT_TRUE(validated_context_c->Assign(*validated_context_b, constraints));
  validated_context_b = nullptr;
  EXPECT_EQ(validated_context_c->GetContext(), context_ptr);
  EXPECT_EQ(validated_context_c->GetValue<int>(), kDefaultInValue);
}

}  // namespace
}  // namespace gb
