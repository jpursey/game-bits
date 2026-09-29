// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/function_hook.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace gb {
namespace {

int Add(int a, int b) { return a + b; }
void Append(std::string* text, std::string_view suffix) {
  text->append(suffix);
}
int TakeValue(std::unique_ptr<int> value) { return *value; }

int (*g_add)(int, int) = &Add;
void (*g_append)(std::string*, std::string_view) = &Append;
int (*g_take_value)(std::unique_ptr<int>) = &TakeValue;
int (*g_not_loaded)(int) = nullptr;

// Records the name it was given for each call, then calls the original.
class RecordHook {
 public:
  RecordHook(std::string name, std::vector<std::string>* calls)
      : name_(std::move(name)), calls_(calls) {}

  template <typename Function, typename... Args>
  auto Call(Function original, Args&&... args) {
    calls_->push_back(name_);
    return original(std::forward<Args>(args)...);
  }

 private:
  std::string name_;
  std::vector<std::string>* calls_;
};

// A second hook type, so it can be stacked with RecordHook on one pointer.
class OuterRecordHook : public RecordHook {
 public:
  using RecordHook::RecordHook;
};

// Doubles the result of the original.
class DoubleHook {
 public:
  template <typename Function, typename... Args>
  static auto Call(Function original, Args&&... args) {
    return 2 * original(std::forward<Args>(args)...);
  }
};

// Stands in for a function that was never loaded, returning a fixed value.
class StubHook {
 public:
  static constexpr bool kHookNotLoaded = true;

  explicit StubHook(std::string_view name) : name_(name) {}

  std::string_view last_call() const { return last_call_; }

  template <typename Function, typename... Args>
  int Call(Function original, Args&&... args) {
    EXPECT_EQ(original, nullptr);
    last_call_ = name_;
    return -1;
  }

 private:
  std::string_view name_;
  std::string_view last_call_;
};

TEST(FunctionHookTest, CallsHookWithOriginalAndArguments) {
  std::vector<std::string> calls;
  {
    FunctionHook<&g_add, RecordHook> hook("add", &calls);
    EXPECT_NE(g_add, &Add);
    EXPECT_EQ(g_add(2, 3), 5);
    EXPECT_EQ(calls, std::vector<std::string>({"add"}));
  }
  EXPECT_EQ(g_add, &Add);
  EXPECT_EQ(g_add(2, 3), 5);
  EXPECT_EQ(calls, std::vector<std::string>({"add"}));
}

TEST(FunctionHookTest, ReturnsHookResult) {
  FunctionHook<&g_add, DoubleHook> hook;
  EXPECT_EQ(g_add(2, 3), 10);
}

TEST(FunctionHookTest, VoidFunctionWithOutputParameter) {
  std::vector<std::string> calls;
  FunctionHook<&g_append, RecordHook> hook("append", &calls);
  std::string text = "a";
  g_append(&text, "b");
  EXPECT_EQ(text, "ab");
  EXPECT_EQ(calls, std::vector<std::string>({"append"}));
}

TEST(FunctionHookTest, MoveOnlyArgument) {
  std::vector<std::string> calls;
  FunctionHook<&g_take_value, RecordHook> hook("take_value", &calls);
  EXPECT_EQ(g_take_value(std::make_unique<int>(7)), 7);
  EXPECT_EQ(calls, std::vector<std::string>({"take_value"}));
}

TEST(FunctionHookTest, HooksStack) {
  std::vector<std::string> calls;
  {
    FunctionHook<&g_add, RecordHook> inner("inner", &calls);
    {
      FunctionHook<&g_add, DoubleHook> middle;
      FunctionHook<&g_add, OuterRecordHook> outer("outer", &calls);
      EXPECT_EQ(g_add(2, 3), 10);
      EXPECT_EQ(calls, std::vector<std::string>({"outer", "inner"}));
    }
    calls.clear();
    EXPECT_EQ(g_add(2, 3), 5);
    EXPECT_EQ(calls, std::vector<std::string>({"inner"}));
  }
  EXPECT_EQ(g_add, &Add);
}

TEST(FunctionHookTest, StandsInForFunctionNotLoaded) {
  {
    FunctionHook<&g_not_loaded, StubHook> hook("not_loaded");
    EXPECT_EQ(g_not_loaded(1), -1);
    EXPECT_EQ(hook.hook().last_call(), "not_loaded");
  }
  EXPECT_EQ(g_not_loaded, nullptr);
}

TEST(FunctionHookTest, LeavesFunctionNotLoadedNull) {
  std::vector<std::string> calls;
  {
    FunctionHook<&g_not_loaded, RecordHook> inner("inner", &calls);
    EXPECT_EQ(g_not_loaded, nullptr);
    {
      FunctionHook<&g_not_loaded, StubHook> outer("outer");
      EXPECT_EQ(g_not_loaded(1), -1);
      EXPECT_EQ(outer.hook().last_call(), "outer");
    }
    EXPECT_EQ(g_not_loaded, nullptr);
  }
  EXPECT_EQ(g_not_loaded, nullptr);
  EXPECT_TRUE(calls.empty());
}

// Aliases, as template argument lists can't be passed to EXPECT_DEATH.
using AddRecordHook = FunctionHook<&g_add, RecordHook>;
using AddDoubleHook = FunctionHook<&g_add, DoubleHook>;
using NotLoadedRecordHook = FunctionHook<&g_not_loaded, RecordHook>;
using NotLoadedStubHook = FunctionHook<&g_not_loaded, StubHook>;

TEST(FunctionHookDeathTest, SameHookTwice) {
  AddDoubleHook hook;
  EXPECT_DEATH(AddDoubleHook(), "Only one FunctionHook");
}

TEST(FunctionHookDeathTest, DestroyedOutOfOrder) {
  EXPECT_DEATH(
      {
        std::vector<std::string> calls;
        auto inner = std::make_unique<AddRecordHook>("inner", &calls);
        AddDoubleHook outer;
        inner.reset();
      },
      "reverse order");
}

TEST(FunctionHookDeathTest, HookLeftNullDestroyedOutOfOrder) {
  EXPECT_DEATH(
      {
        std::vector<std::string> calls;
        auto inner = std::make_unique<NotLoadedRecordHook>("inner", &calls);
        NotLoadedStubHook outer("outer");
        inner.reset();
      },
      "reverse order");
}

}  // namespace
}  // namespace gb
