#include "gb/base/type_info.h"

#include "absl/strings/match.h"
#include "gb/test/thread_tester.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

struct PartialType;
struct StructType {};
class ClassType {};
enum class EnumType { kValueZero, kValueOne, kValueTwo };

struct Counts {
  Counts() = default;
  int destruct = 0;
  int copy_construct = 0;
};

class Item final {
 public:
  Item(Counts* counts) : counts(counts) {}
  Item(const Item& other) : counts(other.counts) { ++counts->copy_construct; }
  ~Item() { ++counts->destruct; }

 private:
  Counts* counts;
};

class NoCopyItem final {
 public:
  NoCopyItem(Counts* counts) : counts(counts) {}
  NoCopyItem(const NoCopyItem&) = delete;
  NoCopyItem& operator=(const NoCopyItem&) = delete;
  NoCopyItem(NoCopyItem&& other) : counts(other.counts){};
  ~NoCopyItem() { ++counts->destruct; }

 private:
  Counts* counts;
};

class NoDestructItem final {
 public:
  NoDestructItem(Counts* counts) : counts(counts) {}
  void Destroy() { delete this; }

 private:
  ~NoDestructItem() { ++counts->destruct; }
  Counts* counts;
};

TEST(TypeKeyTest, KeyIsNotNull) {
  EXPECT_NE(TypeKey::Get<PartialType>(), nullptr);
  EXPECT_NE(TypeKey::Get<StructType>(), nullptr);
  EXPECT_NE(TypeKey::Get<ClassType>(), nullptr);
  EXPECT_NE(TypeKey::Get<EnumType>(), nullptr);
  EXPECT_NE(TypeKey::Get<int>(), nullptr);
  EXPECT_NE(TypeKey::Get<float>(), nullptr);
  EXPECT_NE(TypeKey::Get<bool>(), nullptr);
  EXPECT_NE(TypeKey::Get<void>(), nullptr);
  EXPECT_NE(TypeKey::Get<void*>(), nullptr);
}

TEST(TypeKeyTest, KeyIsStable) {
  EXPECT_EQ(TypeKey::Get<PartialType>(), TypeKey::Get<PartialType>());
  EXPECT_EQ(TypeKey::Get<StructType>(), TypeKey::Get<StructType>());
  EXPECT_EQ(TypeKey::Get<ClassType>(), TypeKey::Get<ClassType>());
  EXPECT_EQ(TypeKey::Get<EnumType>(), TypeKey::Get<EnumType>());
  EXPECT_EQ(TypeKey::Get<int>(), TypeKey::Get<int>());
  EXPECT_EQ(TypeKey::Get<float>(), TypeKey::Get<float>());
  EXPECT_EQ(TypeKey::Get<bool>(), TypeKey::Get<bool>());
  EXPECT_EQ(TypeKey::Get<void>(), TypeKey::Get<void>());
  EXPECT_EQ(TypeKey::Get<void*>(), TypeKey::Get<void*>());
}

TEST(TypeKeyTest, DefaultTypeName) {
  EXPECT_STREQ(TypeKey::Get<PartialType>()->GetTypeName(), "");
  EXPECT_STREQ(TypeKey::Get<ClassType>()->GetTypeName(), "");
}

TEST(TypeKeyTest, SetPartialTypeName) {
  struct PartialTypeForSetName;
  TypeKey::Get<PartialTypeForSetName>()->SetTypeName("PartialTypeForSetName");
  EXPECT_STREQ(TypeKey::Get<PartialTypeForSetName>()->GetTypeName(),
               "PartialTypeForSetName");
}

TEST(TypeKeyTest, SetTypeName) {
  struct TestType {};
  TypeKey::Get<TestType>()->SetTypeName(">>>TestType<<<");
  EXPECT_STREQ(TypeKey::Get<TestType>()->GetTypeName(), ">>>TestType<<<");
}

TEST(TypeKeyTest, SetTypeNameThreadAbuse) {
  struct TestType {};
  ThreadTester tester;
  tester.RunLoop(1, "set-a", []() {
    TypeKey::Get<TestType>()->SetTypeName("A");
    return true;
  });
  tester.RunLoop(1, "set-b", []() {
    TypeKey::Get<TestType>()->SetTypeName("B");
    return true;
  });
  tester.RunLoop(1, "get", []() {
    std::string name = TypeKey::Get<TestType>()->GetTypeName();
    return (name == "A" || name == "B");
  });
  absl::SleepFor(absl::Seconds(1));
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
}

TEST(TypeInfoTest, InfoIsNotNull) {
  EXPECT_NE(TypeInfo::Get<StructType>(), nullptr);
  EXPECT_NE(TypeInfo::Get<ClassType>(), nullptr);
  EXPECT_NE(TypeInfo::Get<EnumType>(), nullptr);
  EXPECT_NE(TypeInfo::Get<int>(), nullptr);
  EXPECT_NE(TypeInfo::Get<float>(), nullptr);
  EXPECT_NE(TypeInfo::Get<bool>(), nullptr);
  EXPECT_NE(TypeInfo::Get<void>(), nullptr);
  EXPECT_NE(TypeInfo::Get<void*>(), nullptr);
}

TEST(TypeInfoTest, InfoIsStable) {
  EXPECT_EQ(TypeInfo::Get<StructType>(), TypeInfo::Get<StructType>());
}

TEST(TypeInfoTest, PlaceholderInfoIsNotNull) {
  EXPECT_NE(TypeInfo::GetPlaceholder<PartialType>(), nullptr);
  EXPECT_NE(TypeInfo::GetPlaceholder<StructType>(), nullptr);
  EXPECT_NE(TypeInfo::GetPlaceholder<ClassType>(), nullptr);
  EXPECT_NE(TypeInfo::GetPlaceholder<EnumType>(), nullptr);
  EXPECT_NE(TypeInfo::GetPlaceholder<int>(), nullptr);
  EXPECT_NE(TypeInfo::GetPlaceholder<float>(), nullptr);
  EXPECT_NE(TypeInfo::GetPlaceholder<bool>(), nullptr);
  EXPECT_NE(TypeInfo::GetPlaceholder<void>(), nullptr);
  EXPECT_NE(TypeInfo::GetPlaceholder<void*>(), nullptr);
}

TEST(TypeInfoTest, PlaceholderCannotDestroy) {
  EXPECT_FALSE(TypeInfo::GetPlaceholder<PartialType>()->CanDestroy());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<StructType>()->CanDestroy());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<ClassType>()->CanDestroy());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<EnumType>()->CanDestroy());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<int>()->CanDestroy());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<float>()->CanDestroy());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<bool>()->CanDestroy());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<void>()->CanDestroy());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<void*>()->CanDestroy());
}

TEST(TypeInfoTest, PlaceholderCannotClone) {
  EXPECT_FALSE(TypeInfo::GetPlaceholder<PartialType>()->CanClone());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<StructType>()->CanClone());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<ClassType>()->CanClone());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<EnumType>()->CanClone());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<int>()->CanClone());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<float>()->CanClone());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<bool>()->CanClone());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<void>()->CanClone());
  EXPECT_FALSE(TypeInfo::GetPlaceholder<void*>()->CanClone());
}

TEST(TypeInfoTest, PlaceholderInfoIsStable) {
  EXPECT_EQ(TypeInfo::GetPlaceholder<PartialType>(),
            TypeInfo::GetPlaceholder<PartialType>());
}

TEST(TypeInfoTest, PlaceholderInfoIsNotInfo) {
  EXPECT_NE(TypeInfo::GetPlaceholder<StructType>(),
            TypeInfo::Get<StructType>());
}

TEST(TypeInfoTest, PlayerholderInfoForPartialType) {
  EXPECT_NE(TypeInfo::GetPlaceholder<PartialType>(), nullptr);
  EXPECT_EQ(TypeInfo::GetPlaceholder<PartialType>(),
            TypeInfo::GetPlaceholder<PartialType>());
}

TEST(TypeInfoTest, KeyMatches) {
  EXPECT_EQ(TypeInfo::Get<StructType>()->Key(), TypeKey::Get<StructType>());
  EXPECT_EQ(TypeInfo::GetPlaceholder<PartialType>()->Key(),
            TypeKey::Get<PartialType>());
}

TEST(TypeInfoTest, DefaultTypeName) {
  EXPECT_TRUE(absl::StrContains(TypeKey::Get<StructType>()->GetTypeName(),
                                "StructType"))
      << "Name is \"" << TypeKey::Get<StructType>()->GetTypeName() << "\"";
}

TEST(TypeInfoTest, GetNameMatchesKey) {
  EXPECT_EQ(TypeInfo::Get<StructType>()->GetTypeName(),
            TypeKey::Get<StructType>()->GetTypeName());
  EXPECT_EQ(TypeInfo::GetPlaceholder<PartialType>()->GetTypeName(),
            TypeKey::Get<PartialType>()->GetTypeName());
}

TEST(TypeInfoTest, SetNameMatches) {
  struct TestType {};
  TypeInfo::Get<StructType>()->SetTypeName("A");
  EXPECT_STREQ(TypeInfo::Get<StructType>()->GetTypeName(), "A");
  EXPECT_STREQ(TypeInfo::GetPlaceholder<StructType>()->GetTypeName(), "A");
  EXPECT_STREQ(TypeKey::Get<StructType>()->GetTypeName(), "A");

  struct PartialTypeForSetName;
  TypeInfo::GetPlaceholder<PartialTypeForSetName>()->SetTypeName("B");
  EXPECT_STREQ(TypeInfo::GetPlaceholder<PartialTypeForSetName>()->GetTypeName(),
               "B");
  EXPECT_STREQ(TypeKey::Get<PartialTypeForSetName>()->GetTypeName(), "B");
}

TEST(TypeInfoTest, InfoCanDestroy) {
  EXPECT_TRUE(TypeInfo::Get<StructType>()->CanDestroy());
  EXPECT_TRUE(TypeInfo::Get<ClassType>()->CanDestroy());
  EXPECT_TRUE(TypeInfo::Get<EnumType>()->CanDestroy());
  EXPECT_TRUE(TypeInfo::Get<int>()->CanDestroy());
  EXPECT_TRUE(TypeInfo::Get<float>()->CanDestroy());
  EXPECT_TRUE(TypeInfo::Get<bool>()->CanDestroy());
  EXPECT_FALSE(TypeInfo::Get<void>()->CanDestroy());
  EXPECT_TRUE(TypeInfo::Get<Item>()->CanDestroy());
  EXPECT_FALSE(TypeInfo::Get<NoDestructItem>()->CanDestroy());
}

TEST(TypeInfoTest, InfoCanClone) {
  EXPECT_TRUE(TypeInfo::Get<StructType>()->CanClone());
  EXPECT_TRUE(TypeInfo::Get<ClassType>()->CanClone());
  EXPECT_TRUE(TypeInfo::Get<EnumType>()->CanClone());
  EXPECT_TRUE(TypeInfo::Get<int>()->CanClone());
  EXPECT_TRUE(TypeInfo::Get<float>()->CanClone());
  EXPECT_TRUE(TypeInfo::Get<bool>()->CanClone());
  EXPECT_FALSE(TypeInfo::Get<void>()->CanClone());
  EXPECT_TRUE(TypeInfo::Get<Item>()->CanClone());
  EXPECT_FALSE(TypeInfo::Get<NoCopyItem>()->CanClone());
}

TEST(TypeInfoTest, InfoAnonymousDestroy) {
  Counts counts = {};
  Item* item = new Item(&counts);
  void* anonymous_item = item;
  TypeInfo::Get<Item>()->Destroy(anonymous_item);
  EXPECT_EQ(counts.destruct, 1);
}

TEST(TypeInfoTest, InfoAnonymousDestroyNull) {
  TypeInfo::Get<Item>()->Destroy(nullptr);
}

TEST(TypeInfoTest, InfoAnonymousNoDestructDestroy) {
  Counts counts = {};
  NoDestructItem* item = new NoDestructItem(&counts);
  void* anonymous_item = item;
  TypeInfo::Get<NoDestructItem>()->Destroy(anonymous_item);
  EXPECT_EQ(counts.destruct, 0);
  item->Destroy();
}

TEST(TypeInfoTest, InfoAnonymousClone) {
  Counts counts = {};
  Item* item = new Item(&counts);
  void* anonymous_item = item;
  Item* cloned_item =
      static_cast<Item*>(TypeInfo::Get<Item>()->Clone(anonymous_item));
  EXPECT_NE(item, cloned_item);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.destruct, 0);
  delete item;
  delete cloned_item;
}

TEST(TypeInfoTest, InfoAnonymousCloneNull) {
  EXPECT_EQ(TypeInfo::Get<Item>()->Clone(nullptr), nullptr);
}

TEST(TypeInfoTest, InfoAnonymousNoCopyClone) {
  Counts counts = {};
  NoCopyItem* item = new NoCopyItem(&counts);
  void* anonymous_item = item;
  EXPECT_EQ(TypeInfo::Get<NoCopyItem>()->Clone(anonymous_item), nullptr);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.destruct, 0);
  delete item;
}

TEST(TypeInfoTest, InfoAnyClone) {
  Counts counts = {};
  std::any any_item = Item(&counts);
  counts.copy_construct = 0;
  counts.destruct = 0;
  Item* cloned_item =
      static_cast<Item*>(TypeInfo::Get<Item>()->Clone(any_item));
  EXPECT_NE(std::any_cast<Item>(&any_item), cloned_item);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.destruct, 0);
  delete cloned_item;
}

TEST(TypeInfoTest, InfoAnyCloneNull) {
  std::any any_item;
  EXPECT_EQ(TypeInfo::Get<Item>()->Clone(any_item), nullptr);
}

TEST(TypeInfoTest, InfoAnyCloneInvalidType) {
  std::any any_item = 5;
  EXPECT_EQ(TypeInfo::Get<Item>()->Clone(any_item), nullptr);
}

TEST(TypeInfoTest, PlaceholderAnonymousDestroy) {
  Counts counts = {};
  Item* item = new Item(&counts);
  void* anonymous_item = item;
  TypeInfo::GetPlaceholder<Item>()->Destroy(anonymous_item);
  EXPECT_EQ(counts.destruct, 0);
  delete item;
}

TEST(TypeInfoTest, PlaceholderAnonymousDestroyNull) {
  TypeInfo::GetPlaceholder<Item>()->Destroy(nullptr);
}

TEST(TypeInfoTest, PlaceholderAnonymousNoDestructDestroy) {
  Counts counts = {};
  NoDestructItem* item = new NoDestructItem(&counts);
  void* anonymous_item = item;
  TypeInfo::GetPlaceholder<NoDestructItem>()->Destroy(anonymous_item);
  EXPECT_EQ(counts.destruct, 0);
  item->Destroy();
}

TEST(TypeInfoTest, PlaceholderAnonymousClone) {
  Counts counts = {};
  Item* item = new Item(&counts);
  void* anonymous_item = item;
  EXPECT_EQ(TypeInfo::GetPlaceholder<Item>()->Clone(anonymous_item), nullptr);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.destruct, 0);
  delete item;
}

TEST(TypeInfoTest, PlaceholderAnonymousCloneNull) {
  EXPECT_EQ(TypeInfo::GetPlaceholder<Item>()->Clone(nullptr), nullptr);
}

TEST(TypeInfoTest, PlaceholderAnonymousNoCopyClone) {
  Counts counts = {};
  NoCopyItem* item = new NoCopyItem(&counts);
  void* anonymous_item = item;
  EXPECT_EQ(TypeInfo::GetPlaceholder<NoCopyItem>()->Clone(anonymous_item),
            nullptr);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.destruct, 0);
  delete item;
}

TEST(TypeInfoTest, PlaceholderAnyClone) {
  Counts counts = {};
  std::any any_item = Item(&counts);
  counts.copy_construct = 0;
  counts.destruct = 0;
  EXPECT_EQ(TypeInfo::GetPlaceholder<NoCopyItem>()->Clone(any_item), nullptr);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.destruct, 0);
}

TEST(TypeInfoTest, PlaceholderAnyCloneNull) {
  std::any any_item;
  EXPECT_EQ(TypeInfo::GetPlaceholder<Item>()->Clone(any_item), nullptr);
}

TEST(TypeInfoTest, PlaceholderAnyCloneInvalidType) {
  std::any any_item = 5;
  EXPECT_EQ(TypeInfo::GetPlaceholder<Item>()->Clone(any_item), nullptr);
}

}  // namespace
}  // namespace gb
