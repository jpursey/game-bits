#include "gb/base/callback_scope.h"

#include <atomic>

#include "gb/test/thread_tester.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

void SetValue(int* value, int new_value) { *value = new_value; }

struct SetValueFunctor {
  explicit SetValueFunctor(int* value) : value_(value) {}
  void operator()(int new_value) { *value_ = new_value; }
  int* const value_;
};

int AddValue(int a, int b) { return a + b; }

struct AddValueFunctor {
  explicit AddValueFunctor(int value) : value_(value) {}
  int operator()(int value) { return value + value_; }
  int const value_;
};

TEST(CallbackScopeTest, VoidCallbackWorksWithFunctionPointer) {
  CallbackScope scope;
  int value = 0;
  auto callback = scope.New<void(int*, int)>(SetValue);
  callback(&value, 5);
  EXPECT_EQ(value, 5);
}

TEST(CallbackScopeTest, VoidCallbackWorksWithFunctor) {
  CallbackScope scope;
  int value = 0;
  auto callback = scope.New<void(int)>(SetValueFunctor(&value));
  callback(5);
  EXPECT_EQ(value, 5);
}

TEST(CallbackScopeTest, VoidCallbackWorksWithLambda) {
  CallbackScope scope;
  int value = 0;
  auto callback =
      scope.New<void(int)>([&value](int new_value) { value = new_value; });
  callback(5);
  EXPECT_EQ(value, 5);
}

TEST(CallbackScopeTest, VoidCallbackWorksWithCallback) {
  CallbackScope scope;
  int value = 0;
  Callback<void(int)> in_callback = [&value](int new_value) {
    value = new_value;
  };
  auto callback = scope.New<void(int)>(std::move(in_callback));
  callback(5);
  EXPECT_EQ(value, 5);
}

TEST(CallbackScopeTest, VoidCallbackFallbackWorks) {
  int value = 0;
  Callback<void(int*, int)> callback;
  {
    CallbackScope scope;
    callback = scope.New<void(int*, int)>(SetValue);
  }
  callback(&value, 5);
  EXPECT_EQ(value, 0);
}

TEST(CallbackScopeTest, CallbackWorksWithFunctionPointer) {
  CallbackScope scope;
  auto callback = scope.New<int(int, int)>(AddValue);
  EXPECT_EQ(callback(1, 2), 3);
}

TEST(CallbackScopeTest, CallbackWorksWithFunctor) {
  CallbackScope scope;
  auto callback = scope.New<int(int)>(AddValueFunctor(1));
  EXPECT_EQ(callback(2), 3);
}

TEST(CallbackScopeTest, CallbackWorksWithLambda) {
  CallbackScope scope;
  int value = 1;
  auto callback = scope.New<int(int)>(
      [&value](int other_value) { return value + other_value; });
  EXPECT_EQ(callback(2), 3);
}

TEST(CallbackScopeTest, CallbackWorksWithCallback) {
  CallbackScope scope;
  int value = 1;
  Callback<int(int)> in_callback = [&value](int other_value) {
    return value + other_value;
  };
  auto callback = scope.New<int(int)>(std::move(in_callback));
  EXPECT_EQ(callback(2), 3);
}

TEST(CallbackScopeTest, CallbackWorksFallbackWorks) {
  Callback<int(int, int)> callback;
  {
    CallbackScope scope;
    callback = scope.New<int(int, int)>(AddValue);
  }
  EXPECT_EQ(callback(1, 2), 0);
}

TEST(CallbackScopeTest, CallbackWorksFallbackWorksWithDefault) {
  Callback<int(int, int)> callback;
  {
    CallbackScope scope;
    callback = scope.New<int(int, int)>(AddValue, 42);
  }
  EXPECT_EQ(callback(1, 2), 42);
}

TEST(CallbackScopeTest, CallbackDeletedWithMultipleThreads) {
  auto scope = std::make_unique<CallbackScope>();
  auto count = std::make_unique<std::atomic<int>>(0);
  ThreadTester tester;
  auto callback = scope->New<bool()>(
      [&count]() {
        if (count == nullptr) {
          return false;
        }
        ++*count;
        return true;
      },
      true);
  tester.RunLoop(
      1, "test", [&callback]() { return callback(); },
      ThreadTester::MaxConcurrency());
  absl::SleepFor(absl::Milliseconds(10));
  scope.reset();
  count.reset();
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
}

TEST(CallbackScopeTest, CallbackScopeDestructorWaits) {
  auto scope = std::make_unique<CallbackScope>();
  ThreadTester tester;
  tester.Run("run", [&scope, &tester]() {
    auto callback = scope->New<bool()>(
        [&tester]() {
          tester.Wait(1);
          tester.Signal(2);
          absl::SleepFor(absl::Milliseconds(10));
          return true;
        },
        false);
    return callback();
  });
  tester.RunThenSignal(3, "delete", [&scope, &tester]() {
    tester.Wait(2);
    scope.reset();
    return true;
  });
  tester.Signal(1);
  tester.Wait(3);
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
}

}  // namespace
}  // namespace gb
