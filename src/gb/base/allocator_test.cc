// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/allocator.h"

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

// Allocates from the system allocator, counting the allocations it has made
// that are not yet freed.
class CountingAllocator : public Allocator {
 public:
  CountingAllocator() = default;

  // Also counts in `total_allocs`, which must outlive this. It is taken by
  // reference to test that TsAllocator forwards references.
  explicit CountingAllocator(int& total_allocs)
      : total_allocs_(&total_allocs) {}

  int GetAllocCount() const { return alloc_count_; }

  void* Alloc(size_t size, size_t align) override {
    ++alloc_count_;
    if (total_allocs_ != nullptr) {
      ++*total_allocs_;
    }
    return GetSystemAllocator()->Alloc(size, align);
  }

  void Free(void* ptr) override {
    if (ptr != nullptr) {
      --alloc_count_;
    }
    GetSystemAllocator()->Free(ptr);
  }

 private:
  int alloc_count_ = 0;
  int* const total_allocs_ = nullptr;
};

// Holds the address of the value it was constructed with.
struct HoldsAddress {
  explicit HoldsAddress(const int& value) : address(&value) {}
  const int* const address;
};

TEST(AllocatorTest, SystemAllocatorAligns) {
  Allocator* allocator = GetSystemAllocator();
  for (int align : {1, 2, 4, 8, 16, 32, 64, 128, 256}) {
    void* ptr = allocator->Alloc(1, align);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % align, 0u)
        << "align " << align;
    allocator->Free(ptr);
  }
}

TEST(AllocatorTest, SystemAllocatorDefaultAlignment) {
  Allocator* allocator = GetSystemAllocator();
  void* ptr = allocator->Alloc(1);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(std::max_align_t), 0u);
  allocator->Free(ptr);
}

TEST(AllocatorTest, FreeNull) {
  CountingAllocator allocator;
  allocator.Free(nullptr);
  EXPECT_EQ(allocator.GetAllocCount(), 0);
}

TEST(AllocatorTest, DefaultAllocatorIsSystemAllocator) {
  EXPECT_EQ(GetDefaultAllocator(), GetSystemAllocator());
}

TEST(AllocatorTest, SetDefaultAllocatorAfterGetFails) {
  GetDefaultAllocator();
  CountingAllocator allocator;
  LogRecorder log_recorder;
  SetDefaultAllocator(&allocator);
  EXPECT_EQ(GetDefaultAllocator(), GetSystemAllocator());
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text,
                                HasSubstr("SetDefaultAllocator failed"))));
}

TEST(AllocatorTest, NewAndDelete) {
  CountingAllocator allocator;
  std::string* text = allocator.New<std::string>("text");
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(*text, "text");
  EXPECT_EQ(allocator.GetAllocCount(), 1);
  allocator.Delete(text);
  EXPECT_EQ(allocator.GetAllocCount(), 0);
}

TEST(AllocatorTest, NewPassesReferences) {
  CountingAllocator allocator;
  const int value = 42;
  HoldsAddress* holder = allocator.New<HoldsAddress>(value);
  EXPECT_EQ(holder->address, &value);
  allocator.Delete(holder);
}

TEST(AllocatorTest, NewMovesArguments) {
  CountingAllocator allocator;
  auto value = std::make_unique<int>(42);
  int* value_ptr = value.get();
  auto* moved = allocator.New<std::unique_ptr<int>>(std::move(value));
  EXPECT_EQ(moved->get(), value_ptr);
  allocator.Delete(moved);
}

TEST(AllocatorTest, DeleteNull) {
  CountingAllocator allocator;
  allocator.Delete<std::string>(nullptr);
  allocator.DeleteArray<std::string>(nullptr, 3);
  EXPECT_EQ(allocator.GetAllocCount(), 0);
}

TEST(AllocatorTest, NewArrayConstructsEveryElementFromArguments) {
  CountingAllocator allocator;
  std::string* texts = allocator.NewArray<std::string>(3, std::string("text"));
  ASSERT_NE(texts, nullptr);
  EXPECT_EQ(texts[0], "text");
  EXPECT_EQ(texts[1], "text");
  EXPECT_EQ(texts[2], "text");
  EXPECT_EQ(allocator.GetAllocCount(), 1);
  allocator.DeleteArray(texts, 3);
  EXPECT_EQ(allocator.GetAllocCount(), 0);
}

TEST(AllocatorTest, TsAllocatorPassesReferences) {
  int total_allocs = 0;
  TsAllocator<CountingAllocator> allocator(total_allocs);
  void* ptr = allocator.Alloc(1);
  EXPECT_EQ(total_allocs, 1);
  allocator.Free(ptr);
}

TEST(StdAllocatorTest, DefaultsToDefaultAllocator) {
  EXPECT_TRUE(StdAllocator<int>() == StdAllocator<int>(GetDefaultAllocator()));
}

TEST(StdAllocatorTest, Vector) {
  CountingAllocator allocator;
  {
    std::vector<int, StdAllocator<int>> values{StdAllocator<int>(&allocator)};
    values.push_back(1);
    values.push_back(2);
    EXPECT_THAT(values, ElementsAre(1, 2));
    EXPECT_GT(allocator.GetAllocCount(), 0);
  }
  EXPECT_EQ(allocator.GetAllocCount(), 0);
}

TEST(StdAllocatorTest, MapRebindsAllocator) {
  CountingAllocator allocator;
  {
    using Allocator = StdAllocator<std::pair<const int, std::string>>;
    std::map<int, std::string, std::less<int>, Allocator> values{
        Allocator(&allocator)};
    values[1] = "one";
    values[2] = "two";
    EXPECT_EQ(values[1], "one");
    EXPECT_EQ(values[2], "two");
    EXPECT_GT(allocator.GetAllocCount(), 0);
  }
  EXPECT_EQ(allocator.GetAllocCount(), 0);
}

TEST(StdAllocatorTest, Equality) {
  CountingAllocator allocator_a;
  CountingAllocator allocator_b;
  const StdAllocator<int> int_a(&allocator_a);
  const StdAllocator<int> int_b(&allocator_b);
  const StdAllocator<double> double_a(&allocator_a);
  EXPECT_TRUE(int_a == StdAllocator<int>(&allocator_a));
  EXPECT_FALSE(int_a != StdAllocator<int>(&allocator_a));
  EXPECT_FALSE(int_a == int_b);
  EXPECT_TRUE(int_a != int_b);
  EXPECT_TRUE(int_a == double_a);
  EXPECT_EQ(StdAllocator<double>(int_a), double_a);
}

TEST(StdAllocatorTest, ListMovesWithAllocator) {
  CountingAllocator allocator;
  {
    std::list<int, StdAllocator<int>> values{StdAllocator<int>(&allocator)};
    values.push_back(1);
    std::list<int, StdAllocator<int>> moved_values = std::move(values);
    EXPECT_THAT(moved_values, ElementsAre(1));
    EXPECT_THAT(values, IsEmpty());
  }
  EXPECT_EQ(allocator.GetAllocCount(), 0);
}

CountingAllocator* GetGlobalCountingAllocator() {
  static CountingAllocator* s_allocator = new CountingAllocator;
  return s_allocator;
}

struct CountingAllocatorTraits {
  static Allocator* Get() noexcept { return GetGlobalCountingAllocator(); }
};

template <typename Type>
using CountingStdAllocator = StdGlobalAllocator<Type, CountingAllocatorTraits>;

TEST(StdGlobalAllocatorTest, Vector) {
  const int start_count = GetGlobalCountingAllocator()->GetAllocCount();
  {
    std::vector<int, CountingStdAllocator<int>> values;
    values.push_back(1);
    values.push_back(2);
    EXPECT_THAT(values, ElementsAre(1, 2));
    EXPECT_GT(GetGlobalCountingAllocator()->GetAllocCount(), start_count);
  }
  EXPECT_EQ(GetGlobalCountingAllocator()->GetAllocCount(), start_count);
}

TEST(StdGlobalAllocatorTest, MapRebindsAllocator) {
  const int start_count = GetGlobalCountingAllocator()->GetAllocCount();
  {
    std::map<int, std::string, std::less<int>,
             CountingStdAllocator<std::pair<const int, std::string>>>
        values;
    values[1] = "one";
    EXPECT_EQ(values[1], "one");
    EXPECT_GT(GetGlobalCountingAllocator()->GetAllocCount(), start_count);
  }
  EXPECT_EQ(GetGlobalCountingAllocator()->GetAllocCount(), start_count);
}

TEST(StdGlobalAllocatorTest, Equality) {
  EXPECT_TRUE(CountingStdAllocator<int>() == CountingStdAllocator<int>());
  EXPECT_FALSE(CountingStdAllocator<int>() != CountingStdAllocator<int>());
  EXPECT_TRUE(CountingStdAllocator<int>() == CountingStdAllocator<double>());
}

TEST(StdGlobalAllocatorTest, DefaultTraitsAllocate) {
  StdGlobalAllocator<int> std_allocator;
  int* ptr = std_allocator.allocate(1);
  ASSERT_NE(ptr, nullptr);
  std_allocator.deallocate(ptr, 1);
}

}  // namespace
}  // namespace gb
