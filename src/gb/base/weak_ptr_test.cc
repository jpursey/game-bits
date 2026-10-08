// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/weak_ptr.h"

#include <atomic>
#include <thread>

#include "gb/test/thread_tester.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class DerivedClass final : public WeakScope<DerivedClass> {
 public:
  explicit DerivedClass(int value)
      : WeakScope<DerivedClass>(this), value_(value) {}
  ~DerivedClass() override {
    InvalidateWeakPtrs();
    value_ = 0;
  }

  int GetValue() const { return value_; }

 private:
  int value_ = 0;
};

class AggregateClass final {
 public:
  AggregateClass() : weak_scope_(this) {}
  ~AggregateClass() { weak_scope_.InvalidateWeakPtrs(); }

  template <typename Type>
  WeakPtr<Type> GetWeakPtr() {
    return weak_scope_.GetWeakPtr<Type>();
  }

 private:
  WeakScope<AggregateClass> weak_scope_;
};

TEST(WeakPtrTest, WeakScopeToNull) {
  WeakScope<int> scope(nullptr);
  auto ptr = scope.GetWeakPtr<int>();
  {
    WeakLock<int> lock(&ptr);
    EXPECT_EQ(lock.Get(), nullptr);
    EXPECT_EQ(lock, nullptr);
    EXPECT_EQ(nullptr, lock);
  }
  scope.InvalidateWeakPtrs();
}

TEST(WeakPtrTest, WeakPtrFromUniquePtr) {
  auto instance = std::make_unique<DerivedClass>(42);
  WeakPtr<DerivedClass> ptr(instance);
  WeakLock<DerivedClass> lock(&ptr);
  EXPECT_EQ(lock.Get(), instance.get());
}

TEST(WeakPtrTest, WeakPtrFromSharedPtr) {
  auto instance = std::make_shared<DerivedClass>(42);
  WeakPtr<DerivedClass> ptr(instance);
  WeakLock<DerivedClass> lock(&ptr);
  EXPECT_EQ(lock.Get(), instance.get());
}

TEST(WeakPtrTest, WeakScopeToNonNull) {
  auto instance = std::make_unique<DerivedClass>(42);
  WeakPtr<DerivedClass> ptr(instance.get());
  {
    WeakLock<DerivedClass> lock(&ptr);
    EXPECT_EQ(lock.Get(), instance.get());
    EXPECT_NE(lock, nullptr);
    EXPECT_NE(nullptr, lock);
    EXPECT_EQ((*lock).GetValue(), 42);
    EXPECT_EQ(lock->GetValue(), 42);
    EXPECT_TRUE(lock);
  }
  instance = nullptr;
  {
    WeakLock<DerivedClass> lock(&ptr);
    EXPECT_EQ(lock.Get(), nullptr);
    EXPECT_EQ(lock, nullptr);
    EXPECT_EQ(nullptr, lock);
  }
}

TEST(WeakPtrTest, WeakScopeToAggregate) {
  auto instance = std::make_unique<AggregateClass>();
  WeakPtr<AggregateClass> ptr(instance.get());
  {
    WeakLock<AggregateClass> lock(&ptr);
    EXPECT_EQ(lock.Get(), instance.get());
  }
  instance = nullptr;
  {
    WeakLock<AggregateClass> lock(&ptr);
    EXPECT_EQ(lock.Get(), nullptr);
  }
}

TEST(WeakPtrTest, WeakScopeDeletedBeforeWeakPtr) {
  auto instance = std::make_unique<DerivedClass>(42);
  WeakPtr<DerivedClass> ptr(instance.get());
  instance = nullptr;
  EXPECT_EQ(WeakLock<DerivedClass>(&ptr), nullptr);
}

TEST(WeakPtrTest, InvalidateBlocksOnLock) {
  ThreadTester tester;
  auto instance = std::make_unique<DerivedClass>(42);
  WeakPtr<DerivedClass> ptr(instance.get());
  tester.Run("invalidate", [&tester, &instance]() {
    tester.Wait(1);
    instance = nullptr;
    return true;
  });
  tester.Run("lock", [&tester, ptr]() {
    WeakLock<DerivedClass> lock(&ptr);
    tester.Signal(1);
    EXPECT_NE(lock, nullptr);
    EXPECT_EQ(lock->GetValue(), 42);
    return true;
  });
  tester.Wait(1);
  tester.Complete();
}

TEST(WeakPtrTest, MultipleLocksAllowedAtOnce) {
  DerivedClass instance(42);
  WeakPtr<DerivedClass> ptr(&instance);
  {
    WeakLock lock_a(&ptr);
    WeakLock lock_b(&ptr);
    EXPECT_TRUE(lock_a && lock_b);
    EXPECT_EQ(lock_a.Get(), lock_b.Get());
  }
}

TEST(WeakPtrLock, WeakPtrToBaseClass) {
  DerivedClass instance(42);
  WeakPtr<WeakScope<DerivedClass>> ptr(&instance);
  EXPECT_EQ(ptr.Lock().Get(), static_cast<WeakScope<DerivedClass>*>(&instance));
}

struct FirstBase {
  int first = 1;
};

struct SecondBase {
  int second = 2;
};

class MultipleBases final : public FirstBase,
                            public SecondBase,
                            public WeakScope<MultipleBases> {
 public:
  MultipleBases() : WeakScope<MultipleBases>(this) {}
  ~MultipleBases() override { InvalidateWeakPtrs(); }
};

TEST(WeakPtrDeathTest, WeakPtrToBaseClassAtDifferentAddress) {
  MultipleBases instance;
  EXPECT_DEATH(WeakPtr<SecondBase>{&instance}, "same address");
}

TEST(WeakPtrTest, LockIsNullOnceInvalidateStarts) {
  auto instance = std::make_unique<DerivedClass>(42);
  WeakPtr<DerivedClass> ptr(instance.get());
  WeakLock<DerivedClass> lock = ptr.Lock();
  std::atomic<bool> invalidated = false;
  ThreadTester tester;
  tester.Run("invalidate", [&instance, &invalidated]() {
    instance->InvalidateWeakPtrs();
    invalidated = true;
    return true;
  });

  // This thread holds a lock, so invalidation can't finish until it is
  // released. New locks, including copies of the lock held, must be null
  // rather than wait for invalidation to finish.
  while (ptr.Lock() != nullptr) {
    std::this_thread::yield();
  }
  EXPECT_EQ(WeakLock<DerivedClass>(lock), nullptr);
  EXPECT_FALSE(invalidated);
  EXPECT_EQ(lock->GetValue(), 42);
  lock = WeakLock<DerivedClass>();
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
  EXPECT_TRUE(invalidated);
}

TEST(WeakPtrTest, WeakConstPtr) {
  DerivedClass instance(42);
  WeakPtr<const DerivedClass> ptr(&instance);
  {
    WeakLock<const DerivedClass> lock(&ptr);
    ASSERT_EQ(lock.Get(), &instance);
    EXPECT_EQ(lock->GetValue(), 42);
  }
}

TEST(WeakPtrTest, ThreadAbuse) {
  ThreadTester tester;
  auto instance = std::make_unique<DerivedClass>(42);
  WeakPtr<DerivedClass> ptr(instance.get());
  auto func = [ptr]() {
    WeakLock<DerivedClass> lock(&ptr);
    return lock == nullptr || lock->GetValue() == 42;
  };
  tester.RunLoop(1, "loop", func, ThreadTester::MaxConcurrency());
  ASSERT_TRUE(tester.WaitForLoopCount("loop", 10000));
  instance = nullptr;
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
}

}  // namespace
}  // namespace gb
