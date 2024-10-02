// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/context.h"

#include <string>
#include <vector>

#include "absl/log/log.h"
#include "gb/test/thread_tester.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

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

class ConstructOnlyItem {
 public:
  ConstructOnlyItem(Counts* counts) : counts(counts) { ++counts->construct; }
  ConstructOnlyItem(const ConstructOnlyItem& other) : counts(other.counts) {
    ++counts->copy_construct;
  }
  ConstructOnlyItem(ConstructOnlyItem&& other) : counts(other.counts) {
    ++counts->move_construct;
  }
  ConstructOnlyItem& operator=(const ConstructOnlyItem& other) = delete;
  ConstructOnlyItem& operator=(ConstructOnlyItem&& other) = delete;
  ~ConstructOnlyItem() { ++counts->destruct; }

 private:
  Counts* counts;
};

class MoveOnlyItem {
 public:
  MoveOnlyItem(Counts* counts) : counts(counts) { ++counts->construct; }
  MoveOnlyItem(const MoveOnlyItem& other) = delete;
  MoveOnlyItem(MoveOnlyItem&& other) : counts(other.counts) {
    ++counts->move_construct;
  }
  MoveOnlyItem& operator=(const MoveOnlyItem& other) = delete;
  MoveOnlyItem& operator=(MoveOnlyItem&& other) {
    ++counts->move_assign;
    return *this;
  }
  ~MoveOnlyItem() { ++counts->destruct; }

 private:
  Counts* counts;
};

class CopyOnlyItem {
 public:
  CopyOnlyItem(Counts* counts) : counts(counts) { ++counts->construct; }
  CopyOnlyItem(const CopyOnlyItem& other) { ++counts->copy_construct; }
  CopyOnlyItem(CopyOnlyItem&& other) = delete;
  CopyOnlyItem& operator=(const CopyOnlyItem& other) {
    ++counts->copy_assign;
    return *this;
  }
  CopyOnlyItem& operator=(CopyOnlyItem&& other) = delete;
  ~CopyOnlyItem() { ++counts->destruct; }

 private:
  Counts* counts;
};

class DeleteOnlyItem {
 public:
  DeleteOnlyItem(const DeleteOnlyItem&) = delete;
  DeleteOnlyItem(DeleteOnlyItem&&) = delete;
  DeleteOnlyItem& operator=(const DeleteOnlyItem&) = delete;
  DeleteOnlyItem& operator=(DeleteOnlyItem&&) = delete;
  ~DeleteOnlyItem() = default;

  static std::unique_ptr<DeleteOnlyItem> New() {
    return std::unique_ptr<DeleteOnlyItem>(new DeleteOnlyItem());
  }

 private:
  DeleteOnlyItem() = default;
};

class DefaultConstructItem {
 public:
  DefaultConstructItem() = default;
  DefaultConstructItem(const DefaultConstructItem&) = delete;
  DefaultConstructItem(DefaultConstructItem&&) = delete;
  DefaultConstructItem& operator=(const DefaultConstructItem&) = delete;
  DefaultConstructItem& operator=(DefaultConstructItem&&) = delete;
  ~DefaultConstructItem() = default;
};

TEST(ContextTest, ConstructEmpty) {
  Context context;
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, NothingExistsInitially) {
  Context context;
  EXPECT_FALSE(context.Exists<int>());
  EXPECT_FALSE(context.NameExists({}));
  EXPECT_FALSE(context.Exists<int>("int"));
}

TEST(ContextTest, ResetWhenEmptyIsEmpty) {
  Context context;
  context.Reset();
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, SetNewIsNotEmpty) {
  Context context;
  context.SetNew<int>();
  EXPECT_FALSE(context.Empty());
}

TEST(ContextTest, SetNewNameDoesNotExist) {
  Context context;
  context.SetNew<int>();
  EXPECT_FALSE(context.NameExists({}));
}

TEST(ContextTest, SetNewExists) {
  Context context;
  context.SetNew<int>();
  EXPECT_TRUE(context.Exists<int>());
}

TEST(ContextTest, SetNewExistsByContextType) {
  Context context;
  context.SetNew<int>();
  EXPECT_TRUE(context.Exists(TypeKey::Get<int>()));
}

TEST(ContextTest, SetNewOfDifferentTypesWork) {
  Context context;
  context.SetNew<int>(10);
  context.SetNew<std::string>("ten");
  EXPECT_EQ(context.GetValue<int>(), 10);
  EXPECT_EQ(context.GetValue<std::string>(), "ten");
}

TEST(ContextTest, SetNamedNewIsNotEmpty) {
  Context context;
  context.SetNamedNew<int>("zero");
  EXPECT_FALSE(context.Empty());
}

TEST(ContextTest, SetNamedNewExists) {
  Context context;
  context.SetNamedNew<int>("zero");
  EXPECT_TRUE(context.Exists<int>("zero"));
  EXPECT_TRUE(context.NameExists("zero"));
}

TEST(ContextTest, SetNamedNewExistsByContextType) {
  Context context;
  context.SetNamedNew<int>("zero");
  EXPECT_TRUE(context.Exists("zero", TypeKey::Get<int>()));
}

TEST(ContextTest, SetValueOfDifferentTypesWork) {
  Context context;
  context.SetValue<int>(10);
  context.SetValue<std::string>("ten");
  EXPECT_EQ(context.GetValue<int>(), 10);
  EXPECT_EQ(context.GetValue<std::string>(), "ten");
}

TEST(ContextTest, SetNamedValueIsNotEmpty) {
  Context context;
  context.SetValue<int>("zero", 0);
  EXPECT_FALSE(context.Empty());
}

TEST(ContextTest, SetNamedValueExists) {
  Context context;
  context.SetValue<int>("zero", 0);
  EXPECT_TRUE(context.Exists<int>("zero"));
  EXPECT_TRUE(context.NameExists("zero"));
}

TEST(ContextTest, SetNamedValueIsNotUnnamedValue) {
  Context context;
  context.SetValue<int>("ten", 10);
  EXPECT_FALSE(context.Exists<int>());
  EXPECT_TRUE(context.Exists<int>("ten"));
  EXPECT_EQ(context.GetValue<int>(), 0);
  EXPECT_EQ(context.GetValue<int>("ten"), 10);
}

TEST(ContextTest, SetValueOfDifferentNamesWork) {
  Context context;
  context.SetValue<int>("ten", 10);
  context.SetValue<int>("twenty", 20);
  EXPECT_EQ(context.GetValue<int>("ten"), 10);
  EXPECT_EQ(context.GetValue<int>("twenty"), 20);
}

TEST(ContextTest, ResetEmptyWorks) {
  Context context;
  context.Reset();
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, ResetMultipleValuesWork) {
  Context context;
  context.SetNew<int>(10);
  context.SetValue<int>("twenty", 20);
  context.SetNew<std::string>("ten");
  context.Reset();
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, ClearItemWorks) {
  Context context;
  context.SetNew<int>();
  context.Clear<int>();
  EXPECT_FALSE(context.Exists<int>());
}

TEST(ContextTest, ClearNamedItemWorks) {
  Context context;
  context.SetNamedNew<int>("int");
  context.Clear<int>("int");
  EXPECT_FALSE(context.Exists<int>("int"));
}

TEST(ContextTest, ClearItemDoesNotClearNamedItem) {
  Context context;
  context.SetValue<int>("ten", 10);
  context.Clear<int>();
  EXPECT_TRUE(context.Exists<int>("ten"));
}

TEST(ContextTest, ClearLastItemIsEmpty) {
  Context context;
  context.SetNew<int>();
  context.Clear<int>();
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, ClearLastNamedItemIsEmpty) {
  Context context;
  context.SetNamedNew<int>("int");
  context.Clear<int>("int");
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, GetMissingValueIsDefault) {
  Context context;
  EXPECT_EQ(context.GetValue<int>(), 0);
}

TEST(ContextTest, GetMissingNamedValueIsDefault) {
  Context context;
  EXPECT_EQ(context.GetValue<int>("ten"), 0);
}

TEST(ContextTest, GetMissingValueReturnsSpecifiedDefault) {
  Context context;
  EXPECT_EQ(context.GetValueOrDefault<int>(5), 5);
}

TEST(ContextTest, GetMissingNamedValueReturnsSpecifiedDefault) {
  Context context;
  EXPECT_EQ(context.GetValueOrDefault<int>("five", 5), 5);
}

TEST(ContextTest, GetMissingValueReturnsCopyConstruct) {
  Counts counts;
  Context context;
  Item item(&counts);
  context.GetValueOrDefault<Item>(item);
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.move_assign, 0);
  EXPECT_EQ(counts.copy_assign, 0);
}

TEST(ContextTest, GetMissingNamedValueReturnsCopyConstruct) {
  Counts counts;
  Context context;
  Item item(&counts);
  context.GetValueOrDefault<Item>("item", item);
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.move_assign, 0);
  EXPECT_EQ(counts.copy_assign, 0);
}

TEST(ContextTest, GetMissingValueReturnsMoveConstruct) {
  Counts counts;
  Context context;
  Item item(&counts);
  context.GetValueOrDefault<Item>(std::move(item));
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.move_construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_assign, 0);
  EXPECT_EQ(counts.copy_assign, 0);
}

TEST(ContextTest, GetMissingNamedValueReturnsMoveConstruct) {
  Counts counts;
  Context context;
  Item item(&counts);
  context.GetValueOrDefault<Item>("item", std::move(item));
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.move_construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_assign, 0);
  EXPECT_EQ(counts.copy_assign, 0);
}

TEST(ContextTest, GetMissingValueReturnsCustomConstruct) {
  Counts counts;
  Context context;
  context.GetValueOrDefault<Item>(&counts);
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_assign, 0);
  EXPECT_EQ(counts.copy_assign, 0);
}

TEST(ContextTest, GetMissingNamedValueReturnsCustomConstruct) {
  Counts counts;
  Context context;
  context.GetValueOrDefault<Item>("item", &counts);
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_assign, 0);
  EXPECT_EQ(counts.copy_assign, 0);
}

TEST(ContextTest, GetValueDoesNotCreate) {
  Context context;
  context.GetValue<int>();
  EXPECT_FALSE(context.Exists<int>());
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, GetNamedValueDoesNotCreate) {
  Context context;
  context.GetValue<int>("int");
  EXPECT_FALSE(context.Exists<int>());
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, GetValueReturnsValue) {
  Context context;
  context.SetNew<int>(5);
  EXPECT_EQ(context.GetValue<int>(), 5);
}

TEST(ContextTest, GetNamedValueReturnsValue) {
  Context context;
  context.SetNamedNew<int>("five", 5);
  EXPECT_EQ(context.GetValue<int>("five"), 5);
}

TEST(ContextTest, GetValueDoesNotRemove) {
  Context context;
  context.SetNew<int>(5);
  context.GetValue<int>();
  EXPECT_TRUE(context.Exists<int>());
  EXPECT_FALSE(context.Empty());
}

TEST(ContextTest, GetNamedValueDoesNotRemove) {
  Context context;
  context.SetNamedNew<int>("five", 5);
  context.GetValue<int>("five");
  EXPECT_TRUE(context.Exists<int>("five"));
  EXPECT_FALSE(context.Empty());
}

TEST(ContextTest, SetNewWithMultipleArgsWorks) {
  Context context;
  context.SetNew<std::vector<int>>(10, 5);
  auto value = context.GetValue<std::vector<int>>();
  EXPECT_EQ(value.size(), 10);
  EXPECT_EQ(value.back(), 5);
}

TEST(ContextTest, MissingItemIsNotOwned) {
  Context context;
  EXPECT_FALSE(context.Owned<int>());
}

TEST(ContextTest, MissingNamedItemIsNotOwned) {
  Context context;
  EXPECT_FALSE(context.Owned<int>("int"));
}

TEST(ContextTest, SetNewIsOwned) {
  Context context;
  context.SetNew<int>();
  EXPECT_TRUE(context.Owned<int>());
}

TEST(ContextTest, GetPtrIsNullForMissingItem) {
  Context context;
  EXPECT_EQ(context.GetPtr<int>(), nullptr);
}

TEST(ContextTest, GetPtrIsNullForMissingNamedItem) {
  Context context;
  EXPECT_EQ(context.GetPtr<int>("int"), nullptr);
}

TEST(ContextTest, GetPtrReturnsOwnedItem) {
  Context context;
  context.SetNew<int>(5);
  ASSERT_NE(context.GetPtr<int>(), nullptr);
  EXPECT_EQ(*context.GetPtr<int>(), 5);
}

TEST(ContextTest, SetOwnedPassesOwnership) {
  Context context;
  auto value = std::make_unique<int>(5);
  auto value_ptr = value.get();
  context.SetOwned(std::move(value));
  EXPECT_TRUE(context.Owned<int>());
  EXPECT_EQ(value.get(), nullptr);
  EXPECT_EQ(context.GetPtr<int>(), value_ptr);
  EXPECT_EQ(*context.GetPtr<int>(), 5);
}

TEST(ContextTest, SetNamedOwnedPassesOwnership) {
  Context context;
  auto value = std::make_unique<int>(5);
  auto value_ptr = value.get();
  context.SetOwned("five", std::move(value));
  EXPECT_TRUE(context.Owned<int>("five"));
  EXPECT_EQ(value.get(), nullptr);
  EXPECT_EQ(context.GetPtr<int>("five"), value_ptr);
  EXPECT_EQ(*context.GetPtr<int>("five"), 5);
}

TEST(ContextTest, ReleaseOwnership) {
  Context context;
  context.SetNew<int>(5);
  auto value_ptr = context.GetPtr<int>();
  auto value = context.Release<int>();
  EXPECT_FALSE(context.Owned<int>());
  EXPECT_FALSE(context.Exists<int>());
  EXPECT_EQ(context.GetPtr<int>(), nullptr);
  EXPECT_EQ(value.get(), value_ptr);
  EXPECT_EQ(*value, 5);
}

TEST(ContextTest, ReleaseNamedOwnership) {
  Context context;
  context.SetNamedNew<int>("int", 5);
  auto value_ptr = context.GetPtr<int>("int");
  auto value = context.Release<int>("int");
  EXPECT_FALSE(context.Owned<int>("int"));
  EXPECT_FALSE(context.Exists<int>("int"));
  EXPECT_EQ(context.GetPtr<int>("int"), nullptr);
  EXPECT_EQ(value.get(), value_ptr);
  EXPECT_EQ(*value, 5);
}

TEST(ContextTest, SetPtrDoesNotPassOwnership) {
  Context context;
  auto value = std::make_unique<int>(5);
  context.SetPtr<int>(value.get());
  EXPECT_TRUE(context.Exists<int>());
  EXPECT_FALSE(context.Owned<int>());
  EXPECT_EQ(context.GetPtr<int>(), value.get());
}

TEST(ContextTest, SetNamedPtrDoesNotPassOwnership) {
  Context context;
  auto value = std::make_unique<int>(5);
  context.SetPtr<int>("int", value.get());
  EXPECT_TRUE(context.Exists<int>("int"));
  EXPECT_FALSE(context.Owned<int>("int"));
  EXPECT_EQ(context.GetPtr<int>("int"), value.get());
}

TEST(ContextTest, SetNewUsesDefaultConstructor) {
  Context context;
  context.SetNew<DefaultConstructItem>();
}

TEST(ContextTest, SetNamedNewUsesDefaultConstructor) {
  Context context;
  context.SetNamedNew<DefaultConstructItem>("item");
}

TEST(ContextTest, SetNewUsesCopyConstructor) {
  Counts counts;
  Context context;
  Item item(&counts);
  context.SetNew<Item>(item);
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
}

TEST(ContextTest, SetNamedNewUsesCopyConstructor) {
  Counts counts;
  Context context;
  Item item(&counts);
  context.SetNamedNew<Item>("item", item);
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
}

TEST(ContextTest, SetNewUsesMoveConstructor) {
  Counts counts;
  Context context;
  Item item(&counts);
  context.SetNew<Item>(std::move(item));
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 1);
}

TEST(ContextTest, SetNamedNewUsesMoveConstructor) {
  Counts counts;
  Context context;
  Item item(&counts);
  context.SetNamedNew<Item>("item", std::move(item));
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 1);
}

TEST(ContextTest, SetNewUsesCustomConstructor) {
  Counts counts;
  Context context;
  context.SetNew<Item>(&counts);
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
}

TEST(ContextTest, SetNamedNewUsesCustomConstructor) {
  Counts counts;
  Context context;
  context.SetNamedNew<Item>("item", &counts);
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
}

TEST(ContextTest, SetOwnedDoesNotConstruct) {
  Context context;
  context.SetOwned<DeleteOnlyItem>(DeleteOnlyItem::New());
}

TEST(ContextTest, SetNamedOwnedDoesNotConstruct) {
  Context context;
  context.SetOwned<DeleteOnlyItem>("item", DeleteOnlyItem::New());
}

TEST(ContextTest, SetPtrDoesNotConstruct) {
  auto item = DeleteOnlyItem::New();
  Context context;
  context.SetPtr<DeleteOnlyItem>(item.get());
}

TEST(ContextTest, SetNamedPtrDoesNotConstruct) {
  auto item = DeleteOnlyItem::New();
  Context context;
  context.SetPtr<DeleteOnlyItem>("item", item.get());
}

TEST(ContextTest, SetValueUsesCopyConstructorWhenNew) {
  Counts counts;
  ConstructOnlyItem item(&counts);
  Context context;
  context.SetValue<ConstructOnlyItem>(item);
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
}

TEST(ContextTest, SetNamedValueUsesCopyConstructorWhenNew) {
  Counts counts;
  ConstructOnlyItem item(&counts);
  Context context;
  context.SetValue<ConstructOnlyItem>("item", item);
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
}

TEST(ContextTest, SetValueUsesMoveConstructorWhenNew) {
  Counts counts;
  ConstructOnlyItem item(&counts);
  Context context;
  context.SetValue<ConstructOnlyItem>(std::move(item));
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 1);
}

TEST(ContextTest, SetNamedValueUsesMoveConstructorWhenNew) {
  Counts counts;
  ConstructOnlyItem item(&counts);
  Context context;
  context.SetValue<ConstructOnlyItem>("item", std::move(item));
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 1);
}

TEST(ContextTest, SetValueUsesCopyConstructorWhenExists) {
  Counts counts;
  ConstructOnlyItem item(&counts);
  Context context;
  context.SetNew<ConstructOnlyItem>(&counts);
  context.SetValue<ConstructOnlyItem>(item);
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
}

TEST(ContextTest, SetNamedValueUsesCopyConstructorWhenExists) {
  Counts counts;
  ConstructOnlyItem item(&counts);
  Context context;
  context.SetNamedNew<ConstructOnlyItem>("item", &counts);
  context.SetValue<ConstructOnlyItem>("item", item);
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
}

TEST(ContextTest, SetValueUsesMoveConstructorWhenExists) {
  Counts counts;
  ConstructOnlyItem item(&counts);
  Context context;
  context.SetNew<ConstructOnlyItem>(&counts);
  context.SetValue<ConstructOnlyItem>(std::move(item));
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 1);
}

TEST(ContextTest, SetNamedValueUsesMoveConstructorWhenExists) {
  Counts counts;
  ConstructOnlyItem item(&counts);
  Context context;
  context.SetNamedNew<ConstructOnlyItem>("item", &counts);
  context.SetValue<ConstructOnlyItem>("item", std::move(item));
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 1);
}

TEST(ContextTest, SetValueUsesCopyAssignment) {
  Counts counts;
  CopyOnlyItem item(&counts);
  Context context;
  context.SetNew<CopyOnlyItem>(&counts);
  context.SetValue<CopyOnlyItem>(item);
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.copy_assign, 1);
}

TEST(ContextTest, SetNamedValueUsesCopyAssignment) {
  Counts counts;
  CopyOnlyItem item(&counts);
  Context context;
  context.SetNamedNew<CopyOnlyItem>("item", &counts);
  context.SetValue<CopyOnlyItem>("item", item);
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.copy_assign, 1);
}

TEST(ContextTest, SetValueUsesMoveAssignment) {
  Counts counts;
  MoveOnlyItem item(&counts);
  Context context;
  context.SetNew<MoveOnlyItem>(&counts);
  context.SetValue<MoveOnlyItem>(std::move(item));
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.move_assign, 1);
}

TEST(ContextTest, SetNamedValueUsesMoveAssignment) {
  Counts counts;
  MoveOnlyItem item(&counts);
  Context context;
  context.SetNamedNew<MoveOnlyItem>("item", &counts);
  context.SetValue<MoveOnlyItem>("item", std::move(item));
  EXPECT_EQ(counts.construct, 2);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.move_assign, 1);
}

TEST(ContextTest, DestructorDeletesOwnedItems) {
  Counts counts;
  {
    Context context;
    context.SetNew<Item>(&counts);
    EXPECT_EQ(counts.destruct, 0);
  }
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ContextTest, DestructorDeletesOwnedNamedItems) {
  Counts counts;
  {
    Context context;
    context.SetNamedNew<Item>("item", &counts);
    EXPECT_EQ(counts.destruct, 0);
  }
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ContextTest, ResetDeletesOwnedItems) {
  Counts counts;
  Context context;
  context.SetNew<Item>(&counts);
  EXPECT_EQ(counts.destruct, 0);
  context.Reset();
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ContextTest, ResetDeletesOwnedNamedItems) {
  Counts counts;
  Context context;
  context.SetNamedNew<Item>("item", &counts);
  EXPECT_EQ(counts.destruct, 0);
  context.Reset();
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ContextTest, ClearDeletesOwnedItems) {
  Counts counts;
  Context context;
  context.SetNew<Item>(&counts);
  EXPECT_EQ(counts.destruct, 0);
  context.Clear<Item>();
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ContextTest, ClearDeletesOwnedNamedItems) {
  Counts counts;
  Context context;
  context.SetNamedNew<Item>("item", &counts);
  EXPECT_EQ(counts.destruct, 0);
  context.Clear<Item>("item");
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ContextTest, SetNewDeletesPreviousOwnedItems) {
  Counts counts1;
  Counts counts2;
  Context context;
  context.SetNew<Item>(&counts1);
  context.SetNew<Item>(&counts2);
  EXPECT_EQ(counts1.destruct, 1);
  EXPECT_EQ(counts2.destruct, 0);
}

TEST(ContextTest, SetOwnedDeletesPreviousOwnedItems) {
  Counts counts1;
  Counts counts2;
  Context context;
  context.SetNew<Item>(&counts1);
  context.SetOwned<Item>(std::make_unique<Item>(&counts2));
  EXPECT_EQ(counts1.destruct, 1);
  EXPECT_EQ(counts2.destruct, 0);
}

TEST(ContextTest, SetPtrDeletesPreviousOwnedItems) {
  Counts counts1;
  Counts counts2;
  Item item(&counts2);
  Context context;
  context.SetNew<Item>(&counts1);
  context.SetPtr<Item>(&item);
  EXPECT_EQ(counts1.destruct, 1);
  EXPECT_EQ(counts2.destruct, 0);
}

TEST(ContextTest, SetValueDeletesPreviousOwnedItems) {
  Counts counts1;
  Counts counts2;
  ConstructOnlyItem item(&counts2);
  Context context;
  context.SetNew<ConstructOnlyItem>(&counts1);
  context.SetValue<ConstructOnlyItem>(item);
  EXPECT_EQ(counts1.destruct, 1);
  EXPECT_EQ(counts2.destruct, 0);
}

TEST(ContextTest, SetValueDoesNotDeleteExistingItem) {
  Counts counts1;
  Counts counts2;
  Item item(&counts2);
  Context context;
  context.SetNew<Item>(&counts1);
  context.SetValue<Item>(item);
  EXPECT_EQ(counts1.destruct, 0);
  EXPECT_EQ(counts2.destruct, 0);
}

TEST(ContextTest, DestructorDoesNotDeleteUnownedItems) {
  Counts counts;
  auto item = std::make_unique<Item>(&counts);
  {
    Context context;
    context.SetPtr<Item>(item.get());
    EXPECT_EQ(counts.destruct, 0);
  }
  EXPECT_EQ(counts.destruct, 0);
}

TEST(ContextTest, ResetDoesNotDeleteUnownedItems) {
  Counts counts;
  auto item = std::make_unique<Item>(&counts);
  Context context;
  context.SetPtr<Item>(item.get());
  EXPECT_EQ(counts.destruct, 0);
  context.Reset();
  EXPECT_EQ(counts.destruct, 0);
}

TEST(ContextTest, ClearDoesNotDeleteUnownedItems) {
  Counts counts;
  auto item = std::make_unique<Item>(&counts);
  Context context;
  context.SetPtr<Item>(item.get());
  EXPECT_EQ(counts.destruct, 0);
  context.Clear<Item>();
  EXPECT_EQ(counts.destruct, 0);
}

TEST(ContextTest, SetNewDoesNotDeletePreviousUnownedItems) {
  Counts counts1;
  auto item = std::make_unique<Item>(&counts1);
  Counts counts2;
  Context context;
  context.SetPtr<Item>(item.get());
  context.SetNew<Item>(&counts2);
  EXPECT_EQ(counts1.destruct, 0);
  EXPECT_EQ(counts2.destruct, 0);
}

TEST(ContextTest, SetOwnedDoesNotDeletePreviousUnownedItems) {
  Counts counts1;
  auto item = std::make_unique<Item>(&counts1);
  Counts counts2;
  Context context;
  context.SetPtr<Item>(item.get());
  context.SetOwned<Item>(std::make_unique<Item>(&counts2));
  EXPECT_EQ(counts1.destruct, 0);
  EXPECT_EQ(counts2.destruct, 0);
}

TEST(ContextTest, SetPtrDoesNotDeletePreviousUnownedItems) {
  Counts counts1;
  auto item1 = std::make_unique<Item>(&counts1);
  Counts counts2;
  auto item2 = std::make_unique<Item>(&counts2);
  Context context;
  context.SetPtr<Item>(item1.get());
  context.SetPtr<Item>(item2.get());
  EXPECT_EQ(counts1.destruct, 0);
  EXPECT_EQ(counts2.destruct, 0);
}

TEST(ContextTest, SetValueDoesNotDeletePreviousUnownedItems) {
  Counts counts1;
  auto item1 = std::make_unique<ConstructOnlyItem>(&counts1);
  Counts counts2;
  auto item2 = std::make_unique<ConstructOnlyItem>(&counts2);
  Context context;
  context.SetPtr<ConstructOnlyItem>(item1.get());
  context.SetValue<ConstructOnlyItem>(*item2);
  EXPECT_EQ(counts1.destruct, 0);
  EXPECT_EQ(counts2.destruct, 0);
}

TEST(ContextTest, NameCanBeString) {
  std::string key("key");
  Context context;
  context.SetValue<int>(key, 5);
  EXPECT_EQ(context.GetValue<int>(key), 5);
}

TEST(ContextTest, NameCanBeStringView) {
  std::string_view key("key");
  Context context;
  context.SetValue<int>(key, 5);
  EXPECT_EQ(context.GetValue<int>(key), 5);
}

TEST(ContextTest, ContextTakesOwnershipOfName) {
  std::string key("key");
  Context context;
  context.SetValue<int>(key, 5);
  key = "not_key";
  EXPECT_FALSE(context.Exists<int>(key));
  EXPECT_TRUE(context.Exists<int>("key"));
}

TEST(ContextTest, SetNamedValueReplacesPreviousType) {
  Counts counts;
  Context context;
  context.SetNamedNew<Item>("item", &counts);
  context.SetNamedNew<int>("item", 5);
  EXPECT_TRUE(context.NameExists("item"));
  EXPECT_FALSE(context.Exists<Item>("item"));
  EXPECT_TRUE(context.Exists<int>("item"));
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ContextTest, ClearByNameWorks) {
  Context context;
  context.SetValue<int>("int", 10);
  context.ClearName("int");
  EXPECT_FALSE(context.NameExists("int"));
  EXPECT_FALSE(context.Exists<int>("int"));
}

TEST(ContextTest, ClearByNameOnlyAffectsThatName) {
  Context context;
  context.SetValue<int>("int", 10);
  context.ClearName("float");
  EXPECT_TRUE(context.NameExists("int"));
  EXPECT_TRUE(context.Exists<int>("int"));
}

TEST(ContextTest, SetAnyFailsIfWrongType) {
  Context context;
  double value = 10.0;
  std::any any_value(value);
  context.SetAny(TypeInfo::Get<int>(), any_value);
  EXPECT_FALSE(context.Exists<int>());
  EXPECT_FALSE(context.Exists<double>());
}

TEST(ContextTest, SetAnyClearsIfWrongType) {
  Context context;
  double value = 10.0;
  std::any any_value(value);
  context.SetValue<int>(100);
  context.SetValue<double>(200.0);
  context.SetAny(TypeInfo::Get<int>(), any_value);
  EXPECT_FALSE(context.Exists<int>());
  EXPECT_EQ(context.GetValue<double>(), 200.0);
}

TEST(ContextTest, SetAnySucceeds) {
  Counts counts;
  Context context;
  std::any any_value(Item{&counts});
  Counts init_counts = counts;
  context.SetAny(TypeInfo::Get<Item>(), any_value);
  EXPECT_TRUE(context.Exists<Item>());
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct + 1);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST(ContextTest, SetAnyNamedFailsIfWrongType) {
  Context context;
  double value = 10.0;
  std::any any_value(value);
  context.SetAny("any", TypeInfo::Get<int>(), any_value);
  EXPECT_FALSE(context.NameExists("any"));
}

TEST(ContextTest, SetAnyClearsNameIfWrongType) {
  Context context;
  double value = 10.0;
  std::any any_value(value);
  context.SetValue<double>("any", 200.0);
  context.SetAny("any", TypeInfo::Get<int>(), any_value);
  EXPECT_FALSE(context.NameExists("any"));
}

TEST(ContextTest, SetAnyNamedSucceeds) {
  Counts counts;
  Context context;
  std::any any_value(Item{&counts});
  Counts init_counts = counts;
  context.SetAny("any", TypeInfo::Get<Item>(), any_value);
  EXPECT_TRUE(context.Exists<Item>("any"));
  EXPECT_EQ(counts.destruct, init_counts.destruct);
  EXPECT_EQ(counts.construct, init_counts.construct);
  EXPECT_EQ(counts.copy_construct, init_counts.copy_construct + 1);
  EXPECT_EQ(counts.move_construct, init_counts.move_construct);
  EXPECT_EQ(counts.copy_assign, init_counts.copy_assign);
  EXPECT_EQ(counts.move_assign, init_counts.move_assign);
}

TEST(ContextTest, SetAnyReplacesNamedValueOfDifferentType) {
  Context context;
  context.SetValue<double>("any", 200.0);
  int value = 10;
  context.SetAny("any", TypeInfo::Get<int>(), std::any(value));
  EXPECT_EQ(context.GetValue<int>("any"), 10);
  EXPECT_FALSE(context.Exists<double>("any"));
}

TEST(ContextTest, SetNewAccessibleInAllWays) {
  Context context;

  context.SetNew<int>(10);
  EXPECT_TRUE(context.Exists<int>());
  EXPECT_EQ(context.GetValue<int>(), 10);
  ASSERT_NE(context.GetPtr<int>(), nullptr);
  EXPECT_EQ(*context.GetPtr<int>(), 10);

  context.SetNamedNew<int>("name", 20);
  EXPECT_TRUE(context.NameExists("name"));
  EXPECT_TRUE(context.Exists<int>("name"));
  EXPECT_EQ(context.GetValue<int>("name"), 20);
  ASSERT_NE(context.GetPtr<int>("name"), nullptr);
  EXPECT_EQ(*context.GetPtr<int>("name"), 20);
}

TEST(ContextTest, SetOwnedAccessibleInAllWays) {
  Context context;

  context.SetOwned<int>(std::make_unique<int>(10));
  EXPECT_TRUE(context.Exists<int>());
  EXPECT_EQ(context.GetValue<int>(), 10);
  ASSERT_NE(context.GetPtr<int>(), nullptr);
  EXPECT_EQ(*context.GetPtr<int>(), 10);

  context.SetOwned<int>("name", std::make_unique<int>(20));
  EXPECT_TRUE(context.NameExists("name"));
  EXPECT_TRUE(context.Exists<int>("name"));
  EXPECT_EQ(context.GetValue<int>("name"), 20);
  ASSERT_NE(context.GetPtr<int>("name"), nullptr);
  EXPECT_EQ(*context.GetPtr<int>("name"), 20);
}

TEST(ContextTest, SetPtrAccessibleInAllWays) {
  Context context;

  int value = 10;
  context.SetPtr<int>(&value);
  EXPECT_TRUE(context.Exists<int>());
  EXPECT_EQ(context.GetValue<int>(), 10);
  ASSERT_NE(context.GetPtr<int>(), nullptr);
  EXPECT_EQ(*context.GetPtr<int>(), 10);

  int named_value = 20;
  context.SetPtr<int>("name", &named_value);
  EXPECT_TRUE(context.NameExists("name"));
  EXPECT_TRUE(context.Exists<int>("name"));
  EXPECT_EQ(context.GetValue<int>("name"), 20);
  ASSERT_NE(context.GetPtr<int>("name"), nullptr);
  EXPECT_EQ(*context.GetPtr<int>("name"), 20);
}

TEST(ContextTest, SetValueAccessibleInAllWays) {
  Context context;

  context.SetValue<int>(10);
  EXPECT_TRUE(context.Exists<int>());
  EXPECT_EQ(context.GetValue<int>(), 10);
  ASSERT_NE(context.GetPtr<int>(), nullptr);
  EXPECT_EQ(*context.GetPtr<int>(), 10);

  context.SetValue<int>("name", 20);
  EXPECT_TRUE(context.NameExists("name"));
  EXPECT_TRUE(context.Exists<int>("name"));
  EXPECT_EQ(context.GetValue<int>("name"), 20);
  ASSERT_NE(context.GetPtr<int>("name"), nullptr);
  EXPECT_EQ(*context.GetPtr<int>("name"), 20);
}

TEST(ContextTest, SetPtrSupportsForwardDeclaredTypes) {
  int dummy = 10;  // Value does not matter.
  struct ForwardDeclaredType;
  ForwardDeclaredType* ptr = reinterpret_cast<ForwardDeclaredType*>(&dummy);

  Context context;

  context.SetPtr<ForwardDeclaredType>(ptr);
  EXPECT_TRUE(context.Exists<ForwardDeclaredType>());
  EXPECT_EQ(context.GetPtr<ForwardDeclaredType>(), ptr);
  context.Clear<ForwardDeclaredType>();
  EXPECT_FALSE(context.Exists<ForwardDeclaredType>());

  context.SetPtr<ForwardDeclaredType>("name", ptr);
  EXPECT_TRUE(context.Exists<ForwardDeclaredType>("name"));
  EXPECT_EQ(context.GetPtr<ForwardDeclaredType>("name"), ptr);
  context.Clear<ForwardDeclaredType>("name");
  EXPECT_FALSE(context.Exists<ForwardDeclaredType>("name"));

  context.SetPtr<ForwardDeclaredType>("name", ptr);
  EXPECT_TRUE(context.NameExists("name"));
  context.ClearName("name");
  EXPECT_FALSE(context.Exists<ForwardDeclaredType>("name"));
  EXPECT_FALSE(context.NameExists("name"));
}

TEST(ContextTest, GetInParentContext) {
  Context parent;
  parent.SetValue<int>(42);
  parent.SetValue<int>("one", 1);
  Context child;
  child.SetParent(&parent);

  EXPECT_EQ(child.GetPtr<int>(), parent.GetPtr<int>());
  EXPECT_EQ(child.GetValue<int>(), 42);
  EXPECT_EQ(child.GetValueOrDefault<int>(24), 42);
  EXPECT_EQ(child.GetPtr<int>("one"), parent.GetPtr<int>("one"));
  EXPECT_EQ(child.GetValue<int>("one"), 1);
  EXPECT_EQ(child.GetValueOrDefault<int>("one", 2), 1);
  EXPECT_TRUE(child.Exists<int>());
  EXPECT_TRUE(child.Exists<int>("one"));
  EXPECT_TRUE(child.Exists(TypeKey::Get<int>()));
  EXPECT_TRUE(child.Exists("one", TypeKey::Get<int>()));
  EXPECT_TRUE(child.NameExists("one"));
  EXPECT_FALSE(child.Owned<int>());
  EXPECT_FALSE(child.Owned<int>("one"));

  EXPECT_TRUE(parent.Owned<int>());
  EXPECT_TRUE(parent.Owned<int>("one"));

  EXPECT_EQ(child.GetPtr<double>(), nullptr);
  EXPECT_EQ(child.GetValue<double>(), 0.0);
  EXPECT_EQ(child.GetValueOrDefault<double>(2.0), 2.0);
  EXPECT_EQ(child.GetPtr<double>("one"), nullptr);
  EXPECT_EQ(child.GetValue<double>("one"), 0.0);
  EXPECT_EQ(child.GetValueOrDefault<double>("one", 2.0), 2.0);
  EXPECT_FALSE(child.Exists<double>());
  EXPECT_FALSE(child.Exists<double>("one"));
  EXPECT_FALSE(child.Exists(TypeKey::Get<double>()));
  EXPECT_FALSE(child.Exists("one", TypeKey::Get<double>()));
  EXPECT_FALSE(child.NameExists("two"));
}

TEST(ContextTest, ChangeParent) {
  Context parent[2];
  parent[0].SetValue<int>(42);
  parent[1].SetValue<int>(24);
  Context child;

  EXPECT_EQ(child.GetValue<int>(), 0);
  child.SetParent(&parent[0]);
  EXPECT_EQ(child.GetValue<int>(), 42);
  child.SetParent(&parent[1]);
  EXPECT_EQ(child.GetValue<int>(), 24);
  child.SetParent(nullptr);
  EXPECT_EQ(child.GetValue<int>(), 0);
}

TEST(ContextTest, OverrideParentContext) {
  Context parent;
  parent.SetValue<int>(42);
  parent.SetValue<int>("one", 1);
  Context child;
  child.SetParent(&parent);

  child.SetValue<int>(24);
  EXPECT_EQ(child.GetValue<int>(), 24);
  EXPECT_EQ(parent.GetValue<int>(), 42);

  child.SetValue<int>("one", 2);
  EXPECT_EQ(child.GetValue<int>("one"), 2);
  EXPECT_EQ(parent.GetValue<int>("one"), 1);

  child.Clear<int>();
  EXPECT_EQ(child.GetValue<int>(), 42);
  EXPECT_EQ(parent.GetValue<int>(), 42);

  child.Clear<int>("one");
  EXPECT_EQ(child.GetValue<int>("one"), 1);
  EXPECT_EQ(parent.GetValue<int>("one"), 1);

  child.SetValue<int>(24);
  child.SetValue<int>("one", 2);
  EXPECT_EQ(child.GetValue<int>(), 24);
  EXPECT_EQ(child.GetValue<int>("one"), 2);

  child.ClearName("one");
  EXPECT_EQ(child.GetValue<int>("one"), 1);
  EXPECT_EQ(parent.GetValue<int>("one"), 1);

  child.Reset();
  EXPECT_EQ(child.GetValue<int>(), 42);
  EXPECT_EQ(parent.GetValue<int>(), 42);
  EXPECT_EQ(child.GetValue<int>("one"), 1);
  EXPECT_EQ(parent.GetValue<int>("one"), 1);
}

TEST(ContextTest, MultipleChildContexts) {
  Context parent;
  parent.SetValue<int>(42);
  Context child[2];
  child[0].SetParent(&parent);
  child[1].SetParent(&parent);

  EXPECT_EQ(child[0].GetValue<int>(), 42);
  EXPECT_EQ(child[1].GetValue<int>(), 42);
  child[0].SetValue<int>(24);
  EXPECT_EQ(child[0].GetValue<int>(), 24);
  EXPECT_EQ(child[1].GetValue<int>(), 42);
  parent.SetValue<int>(100);
  EXPECT_EQ(child[0].GetValue<int>(), 24);
  EXPECT_EQ(child[1].GetValue<int>(), 100);
}

TEST(ContextTest, ParentDeletion) {
  auto parent = std::make_unique<Context>();
  parent->SetValue<int>(42);
  Context child;
  child.SetParent(parent.get());

  EXPECT_EQ(child.GetValue<int>(), 42);
  parent.reset();
  EXPECT_EQ(child.GetValue<int>(), 0);
}

TEST(ContextTest, ThreadAbuse) {
  Context parent;
  Context context;
  ThreadTester tester;
  int int_ptr = 42;
  auto func = [&parent, &context, &int_ptr]() {
    context.SetParent(&parent);
    context.Empty();
    context.Reset();
    context.SetNew<int>(5);
    parent.SetNew<int>(55);
    context.SetNamedNew<int>("int", 6);
    parent.SetNamedNew<int>("int", 66);
    context.SetOwned<double>(std::make_unique<double>(10.0));
    parent.SetOwned<double>(std::make_unique<double>(101.0));
    context.SetOwned<double>("double", std::make_unique<double>(20.0));
    parent.SetOwned<double>("double", std::make_unique<double>(202.0));
    context.SetPtr<int>(&int_ptr);
    parent.SetPtr<int>(&int_ptr);
    context.SetPtr<int>("int", &int_ptr);
    parent.SetPtr<int>("int", &int_ptr);
    context.SetValue<double>(30.0);
    parent.SetValue<double>(33.0);
    context.SetValue<double>("double", 40.0);
    parent.SetValue<double>("double", 44.0);
    std::any value(100);
    context.SetAny(TypeInfo::Get<int>(), value);
    value = 200;
    context.SetAny("int", TypeInfo::Get<int>(), value);
    context.GetPtr<int>();
    context.GetPtr<int>("int");
    context.GetValue<double>();
    context.GetValue<double>("double");
    context.GetValueOrDefault<double>(42.0);
    context.GetValueOrDefault<double>("double", 42.0);
    context.Exists<int>();
    context.Exists<int>("int");
    context.Exists(TypeKey::Get<double>());
    context.Exists("double", TypeKey::Get<double>());
    context.SetParent(nullptr);
    context.NameExists("int");
    context.Owned<double>();
    context.Owned<double>("double");
    context.Release<double>();
    context.Clear<int>();
    context.Clear<int>("int");
    context.Clear(TypeKey::Get<double>());
    context.Clear("double", TypeKey::Get<double>());
    context.ClearName("int");
    return true;
  };
  tester.RunLoop(1, "loop", func, ThreadTester::MaxConcurrency());
  absl::SleepFor(absl::Seconds(1));
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
}

}  // namespace
}  // namespace gb
