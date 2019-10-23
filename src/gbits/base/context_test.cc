#include "gbits/base/context.h"

#include <string>
#include <vector>

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

TEST(ContextTest, SetNewExists) {
  Context context;
  context.SetNew<int>();
  EXPECT_TRUE(context.Exists<int>());
}

TEST(ContextTest, SetNewOfDifferentTypesWork) {
  Context context;
  context.SetNew<int>(10);
  context.SetNew<std::string>("ten");
  EXPECT_EQ(context.GetValue<int>(), 10);
  EXPECT_EQ(context.GetValue<std::string>(), "ten");
}

TEST(ContextTest, ResetMultipleValuesWork) {
  Context context;
  context.SetNew<int>(10);
  context.SetNew<std::string>("ten");
  context.Reset();
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, ResetIsEmpty) {
  Context context;
  context.SetNew<int>();
  context.Reset();
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, ClearItemWorks) {
  Context context;
  context.SetNew<int>();
  context.Clear<int>();
  EXPECT_FALSE(context.Exists<int>());
}

TEST(ContextTest, ClearLastItemIsEmpty) {
  Context context;
  context.SetNew<int>();
  context.Clear<int>();
  EXPECT_TRUE(context.Empty());
}

TEST(ContextTest, GetMissingValueIsDefault) {
  Context context;
  EXPECT_EQ(context.GetValue<int>(), 0);
}

TEST(ContextTest, GetMissingValueReturnsSpecifiedDefault) {
  Context context;
  EXPECT_EQ(context.GetValue<int>(5), 5);
}

TEST(ContextTest, GetMissingValueReturnsCopyConstruct) {
  Counts counts;
  Context context;
  Item item(&counts);
  context.GetValue<Item>(item);
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
  context.GetValue<Item>(std::move(item));
  EXPECT_EQ(counts.construct, 1);
  EXPECT_EQ(counts.move_construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_assign, 0);
  EXPECT_EQ(counts.copy_assign, 0);
}

TEST(ContextTest, GetMissingValueReturnsCustomConstruct) {
  Counts counts;
  Context context;
  context.GetValue<Item>(&counts);
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

TEST(ContextTest, GetValueReturnsValue) {
  Context context;
  context.SetNew<int>(5);
  EXPECT_EQ(context.GetValue<int>(), 5);
}

TEST(ContextTest, GetValueDoesNotRemove) {
  Context context;
  context.SetNew<int>(5);
  context.GetValue<int>();
  EXPECT_TRUE(context.Exists<int>());
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

TEST(ContextTest, SetNewIsOwned) {
  Context context;
  context.SetNew<int>();
  EXPECT_TRUE(context.Owned<int>());
}

TEST(ContextTest, GetPtrIsNullForMissingItem) {
  Context context;
  EXPECT_EQ(context.GetPtr<int>(), nullptr);
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

TEST(ContextTest, SetPtrDoesNotPassOwnership) {
  Context context;
  auto value = std::make_unique<int>(5);
  context.SetPtr<int>(value.get());
  EXPECT_TRUE(context.Exists<int>());
  EXPECT_FALSE(context.Owned<int>());
  EXPECT_EQ(context.GetPtr<int>(), value.get());
}

TEST(ContextTest, SetNewUsesDefaultConstructor) {
  Context context;
  context.SetNew<DefaultConstructItem>();
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

TEST(ContextTest, SetNewUsesMoveConstructor) {
  Counts counts;
  Context context;
  Item item(&counts);
  context.SetNew<Item>(std::move(item));
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

TEST(ContextTest, SetOwnedDoesNotConstruct) {
  Context context;
  context.SetOwned<DeleteOnlyItem>(DeleteOnlyItem::New());
}

TEST(ContextTest, SetPtrDoesNotConstruct) {
  auto item = DeleteOnlyItem::New();
  Context context;
  context.SetPtr<DeleteOnlyItem>(item.get());
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

TEST(ContextTest, SetValueUsesMoveConstructorWhenNew) {
  Counts counts;
  ConstructOnlyItem item(&counts);
  Context context;
  context.SetValue<ConstructOnlyItem>(std::move(item));
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

TEST(ContextTest, DestructorDeletesOwnedItems) {
  Counts counts;
  {
    Context context;
    context.SetNew<Item>(&counts);
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

TEST(ContextTest, ClearDeletesOwnedItems) {
  Counts counts;
  Context context;
  context.SetNew<Item>(&counts);
  EXPECT_EQ(counts.destruct, 0);
  context.Clear<Item>();
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

}  // namespace
}  // namespace gb
