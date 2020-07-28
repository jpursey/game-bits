#include "gb/base/callback.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

int g_value = 0;
void SetValueTo42() { g_value = 42; }
void SetValue(int value) { g_value = value; }
int GetValue() { return g_value; }
int AddValues(int a, int b) { return a + b; }

class AddValueFunctor {
 public:
  explicit AddValueFunctor(int value) : value_(value) {}

  int operator()(int value) { return value + value_; }

 private:
  const int value_;
};

struct MethodCounterInfo {
  int default_constructor_count_ = 0;
  int copy_constructor_count_ = 0;
  int move_constructor_count_ = 0;
  int copy_assign_count_ = 0;
  int move_assign_count_ = 0;
  int destructor_count_ = 0;
  int call_count_ = 0;
};

class MethodCounter {
 public:
  static MethodCounterInfo& Info() {
    static MethodCounterInfo info;
    return info;
  }
  static void Reset() { Info() = {}; }

  MethodCounter() { Info().default_constructor_count_ += 1; }
  MethodCounter(const MethodCounter&) { Info().copy_constructor_count_ += 1; }
  MethodCounter(MethodCounter&&) { Info().move_constructor_count_ += 1; }
  MethodCounter& operator=(const MethodCounter&) {
    Info().copy_assign_count_ += 1;
    return *this;
  }
  MethodCounter& operator=(MethodCounter&&) {
    Info().move_assign_count_ += 1;
    return *this;
  }
  ~MethodCounter() { Info().destructor_count_ += 1; }
  void operator()() { Info().call_count_ += 1; }
};

TEST(CallbackTest, DefaultConstruct) {
  Callback<void()> callback;
  EXPECT_FALSE(callback);
  EXPECT_EQ(callback, nullptr);
  EXPECT_EQ(nullptr, callback);
}

TEST(CallbackTest, NullConstruct) {
  Callback<void()> callback{nullptr};
  EXPECT_FALSE(callback);
  EXPECT_EQ(callback, nullptr);
  EXPECT_EQ(nullptr, callback);
}

TEST(CallbackTest, NonNullCallback) {
  Callback<void()> callback{SetValueTo42};
  EXPECT_TRUE(callback);
  EXPECT_NE(callback, nullptr);
  EXPECT_NE(nullptr, callback);
}

TEST(CallbackTest, PointerConstructSetValueTo42) {
  g_value = 0;
  Callback<void()> callback{SetValueTo42};
  EXPECT_EQ(g_value, 0);
  callback();
  EXPECT_EQ(g_value, 42);
}

TEST(CallbackTest, PointerConstructSetValue) {
  g_value = 0;
  Callback<void(int)> callback{SetValue};
  EXPECT_EQ(g_value, 0);
  callback(42);
  EXPECT_EQ(g_value, 42);
}

TEST(CallbackTest, PointerConstructGetValue) {
  g_value = 100;
  Callback<int()> callback{GetValue};
  EXPECT_EQ(callback(), 100);
}

TEST(CallbackTest, PointerConstructAddValues) {
  Callback<int(int, int)> callback{AddValues};
  EXPECT_EQ(callback(1, 2), 3);
}

TEST(CallbackTest, PointerConstructAddValueFunctor) {
  AddValueFunctor functor(10);
  Callback<int(int)> callback{&functor};
  EXPECT_EQ(callback(20), 30);
}

TEST(CallbackTest, UniquePointerConstruct) {
  Callback<int(int)> callback{std::make_unique<AddValueFunctor>(10)};
  EXPECT_EQ(callback(20), 30);
}

TEST(CallbackTest, MoveConstructFunctor) {
  Callback<int(int)> callback{AddValueFunctor(10)};
  EXPECT_EQ(callback(20), 30);
}

TEST(CallbackTest, MoveConstruct) {
  Callback<int(int)> callback{AddValueFunctor(10)};
  Callback<int(int)> other_callback(std::move(callback));
  EXPECT_FALSE(callback);
  EXPECT_EQ(other_callback(20), 30);
}

TEST(CallbackTest, LambdaConstruct) {
  int value = 1;
  Callback<int(int)> callback{
      [&value](int new_value) { return value + new_value; }};
  value = 2;
  EXPECT_EQ(callback(3), 5);
}

TEST(CallbackTest, MoveOnlyLambdaConstruct) {
  auto value_ptr = std::make_unique<int>(1);
  auto callable = [value_ptr = std::move(value_ptr)](int new_value) {
    return *value_ptr + new_value;
  };
  Callback<int(int)> callback{std::move(callable)};
  EXPECT_EQ(callback(2), 3);
}

TEST(CallbackTest, StatelessLambdaConstruct) {
  Callback<int(int)> callback{[](int value) { return value + 1; }};
  EXPECT_EQ(callback(2), 3);
}

TEST(CallbackTest, PointerAssignSetValueTo42) {
  g_value = 0;
  Callback<void()> callback;
  callback = SetValueTo42;
  EXPECT_EQ(g_value, 0);
  callback();
  EXPECT_EQ(g_value, 42);
}

TEST(CallbackTest, PointerAssignSetValue) {
  g_value = 0;
  Callback<void(int)> callback;
  callback = SetValue;
  EXPECT_EQ(g_value, 0);
  callback(42);
  EXPECT_EQ(g_value, 42);
}

TEST(CallbackTest, PointerAssignGetValue) {
  g_value = 100;
  Callback<int()> callback;
  callback = GetValue;
  EXPECT_EQ(callback(), 100);
}

TEST(CallbackTest, PointerAssignAddValues) {
  Callback<int(int, int)> callback;
  callback = AddValues;
  EXPECT_EQ(callback(1, 2), 3);
}

TEST(CallbackTest, PointerAssignAddValueFunctor) {
  AddValueFunctor functor(10);
  Callback<int(int)> callback;
  callback = &functor;
  EXPECT_EQ(callback(20), 30);
}

TEST(CallbackTest, UniquePointerAssign) {
  Callback<int(int)> callback;
  callback = std::make_unique<AddValueFunctor>(10);
  EXPECT_EQ(callback(20), 30);
}

TEST(CallbackTest, MoveAssignFunctor) {
  Callback<int(int)> callback;
  callback = AddValueFunctor(10);
  EXPECT_EQ(callback(20), 30);
}

TEST(CallbackTest, MoveAssign) {
  Callback<int(int)> callback{AddValueFunctor(10)};
  Callback<int(int)> other_callback;
  other_callback = std::move(callback);
  EXPECT_FALSE(callback);
  EXPECT_EQ(other_callback(20), 30);
}

TEST(CallbackTest, LambdaAssign) {
  int value = 1;
  Callback<int(int)> callback;
  callback = [&value](int new_value) { return value + new_value; };
  value = 2;
  EXPECT_EQ(callback(3), 5);
}

TEST(CallbackTest, MoveOnlyLambdaAssign) {
  auto value_ptr = std::make_unique<int>(1);
  Callback<int(int)> callback;
  callback = [value_ptr = std::move(value_ptr)](int new_value) {
    return *value_ptr + new_value;
  };
  EXPECT_EQ(callback(2), 3);
}

TEST(CallbackTest, StatelessLambdaAssign) {
  Callback<int(int)> callback;
  callback = [](int value) { return value + 1; };
  EXPECT_EQ(callback(2), 3);
}

TEST(CallbackTest, ConstConstructMethodCounter) {
  MethodCounter::Reset();
  {
    const MethodCounter counter;
    Callback<void()> callback(counter);
  }
  EXPECT_EQ(MethodCounter::Info().default_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().move_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().copy_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().destructor_count_, 2);
  EXPECT_EQ(MethodCounter::Info().call_count_, 0);
}

TEST(CallbackTest, LValueConstructMethodCounter) {
  MethodCounter::Reset();
  {
    MethodCounter counter;
    Callback<void(void)> callback(counter);
  }
  EXPECT_EQ(MethodCounter::Info().default_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().move_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().copy_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().destructor_count_, 2);
  EXPECT_EQ(MethodCounter::Info().call_count_, 0);
}

TEST(CallbackTest, RValueConstructMethodCounter) {
  MethodCounter::Reset();
  {
    MethodCounter counter;
    Callback<void(void)> callback(std::move(counter));
  }
  EXPECT_EQ(MethodCounter::Info().default_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().destructor_count_, 2);
  EXPECT_EQ(MethodCounter::Info().call_count_, 0);
}

TEST(CallbackTest, TemporaryConstructMethodCounter) {
  MethodCounter::Reset();
  { Callback<void(void)> callback(MethodCounter{}); }
  EXPECT_EQ(MethodCounter::Info().default_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().destructor_count_, 2);
  EXPECT_EQ(MethodCounter::Info().call_count_, 0);
}

TEST(CallbackTest, UniquePointerConstructMethodCounter) {
  MethodCounter::Reset();
  { Callback<void(void)> callback(std::make_unique<MethodCounter>()); }
  EXPECT_EQ(MethodCounter::Info().default_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().copy_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().destructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().call_count_, 0);
}

TEST(CallbackTest, SelfAssignMethodCounter) {
  MethodCounter::Reset();
  Callback<void(void)> callback(MethodCounter{});
  callback = std::move(callback);
  EXPECT_EQ(MethodCounter::Info().default_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().destructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().call_count_, 0);
}

TEST(CallbackTest, NullAssignMethodCounter) {
  MethodCounter::Reset();
  Callback<void(void)> callback(MethodCounter{});
  callback = nullptr;
  EXPECT_EQ(MethodCounter::Info().default_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_constructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().copy_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().destructor_count_, 2);
  EXPECT_EQ(MethodCounter::Info().call_count_, 0);
}

TEST(CallbackTest, MoveAssignMethodCounter) {
  MethodCounter::Reset();
  Callback<void(void)> callback(MethodCounter{});
  callback = MethodCounter{};
  EXPECT_EQ(MethodCounter::Info().default_constructor_count_, 2);
  EXPECT_EQ(MethodCounter::Info().copy_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_constructor_count_, 2);
  EXPECT_EQ(MethodCounter::Info().copy_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().destructor_count_, 3);
  EXPECT_EQ(MethodCounter::Info().call_count_, 0);
}

TEST(CallbackTest, UniquePointerAssignMethodCounter) {
  MethodCounter::Reset();
  Callback<void(void)> callback(std::make_unique<MethodCounter>());
  callback = std::make_unique<MethodCounter>();
  EXPECT_EQ(MethodCounter::Info().default_constructor_count_, 2);
  EXPECT_EQ(MethodCounter::Info().copy_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().copy_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().destructor_count_, 1);
  EXPECT_EQ(MethodCounter::Info().call_count_, 0);
}

TEST(CallbackTest, MoveParametersWork) {
  MethodCounter counter;
  MethodCounter::Reset();
  Callback<void(MethodCounter)> callback(
      [](MethodCounter counter) { counter(); });
  callback(std::move(counter));
  EXPECT_EQ(MethodCounter::Info().default_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().copy_constructor_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_constructor_count_, 2);
  EXPECT_EQ(MethodCounter::Info().copy_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().move_assign_count_, 0);
  EXPECT_EQ(MethodCounter::Info().destructor_count_, 2);
  EXPECT_EQ(MethodCounter::Info().call_count_, 1);
}

TEST(CallbackTest, MoveOnlyParametersWork) {
  std::unique_ptr<int> value = std::make_unique<int>(5);
  int* value_ptr = value.get();
  Callback<void(std::unique_ptr<int>)> callback(
      [value_ptr](std::unique_ptr<int> value) {
        EXPECT_EQ(value_ptr, value.get());
      });
  callback(std::move(value));
  EXPECT_EQ(value, nullptr);
}

TEST(CallbackTest, LValueParametersWork) {
  int x = 1;
  int y = 2;
  Callback<int(int, int)> callback(AddValues);
  EXPECT_EQ(callback(x, y), 3);
}

TEST(CallbackTest, TypeConversionParametersWork) {
  int x = 1;
  int y = 2;
  Callback<double(double, double)> callback(
      [](double a, double b) { return a + b; });
  EXPECT_EQ(callback(x, y), 3.0);
}

TEST(CallbackTest, ImplicitCastFromNoCaptureLambdaToCallbackInParameter) {
  struct Type {
    int Call(Callback<int(int)> callback, int value) { return callback(value); }
  };
  Type type;
  EXPECT_EQ(type.Call([](int value) { return value; }, 5), 5);
}

}  // namespace
}  // namespace gb
