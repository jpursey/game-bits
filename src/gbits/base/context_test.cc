#include "gbits/base/context.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace gb {
namespace {

// Helper item to track when and if it is deleted.
struct Item {
  Item(bool* deleted) : deleted(deleted) {}
  ~Item() { *deleted = true; }
  bool* deleted;
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

TEST(ContextTest, DestructorDeletesOwnedItems) {
  bool deleted = false;
  {
    Context context;
    context.SetNew<Item>(&deleted);
    EXPECT_FALSE(deleted);
  }
  EXPECT_TRUE(deleted);
}

TEST(ContextTest, ResetDeletesOwnedItems) {
  bool deleted = false;
  Context context;
  context.SetNew<Item>(&deleted);
  EXPECT_FALSE(deleted);
  context.Reset();
  EXPECT_TRUE(deleted);
}

TEST(ContextTest, ClearDeletesOwnedItems) {
  bool deleted = false;
  Context context;
  context.SetNew<Item>(&deleted);
  EXPECT_FALSE(deleted);
  context.Clear<Item>();
  EXPECT_TRUE(deleted);
}

TEST(ContextTest, SetNewDeletesPreviousOwnedItems) {
  bool deleted1 = false;
  bool deleted2 = false;
  Context context;
  context.SetNew<Item>(&deleted1);
  context.SetNew<Item>(&deleted2);
  EXPECT_TRUE(deleted1);
  EXPECT_FALSE(deleted2);
}

TEST(ContextTest, SetOwnedDeletesPreviousOwnedItems) {
  bool deleted1 = false;
  bool deleted2 = false;
  Context context;
  context.SetNew<Item>(&deleted1);
  context.SetOwned<Item>(std::make_unique<Item>(&deleted2));
  EXPECT_TRUE(deleted1);
  EXPECT_FALSE(deleted2);
}

TEST(ContextTest, SetPtrDeletesPreviousOwnedItems) {
  bool deleted1 = false;
  bool deleted2 = false;
  Item unowned_item(&deleted2);
  Context context;
  context.SetNew<Item>(&deleted1);
  context.SetPtr<Item>(&unowned_item);
  EXPECT_TRUE(deleted1);
  EXPECT_FALSE(deleted2);
}

TEST(ContextTest, DestructorDoesNotDeleteUnownedItems) {
  bool deleted = false;
  auto unowned_item = std::make_unique<Item>(&deleted);
  {
    Context context;
    context.SetPtr<Item>(unowned_item.get());
    EXPECT_FALSE(deleted);
  }
  EXPECT_FALSE(deleted);
}

TEST(ContextTest, ResetDoesNotDeleteUnownedItems) {
  bool deleted = false;
  auto unowned_item = std::make_unique<Item>(&deleted);
  Context context;
  context.SetPtr<Item>(unowned_item.get());
  EXPECT_FALSE(deleted);
  context.Reset();
  EXPECT_FALSE(deleted);
}

TEST(ContextTest, ClearDoesNotDeleteUnownedItems) {
  bool deleted = false;
  auto unowned_item = std::make_unique<Item>(&deleted);
  Context context;
  context.SetPtr<Item>(unowned_item.get());
  EXPECT_FALSE(deleted);
  context.Clear<Item>();
  EXPECT_FALSE(deleted);
}

TEST(ContextTest, SetNewDoesNotDeletePreviousUnownedItems) {
  bool deleted1 = false;
  auto unowned_item = std::make_unique<Item>(&deleted1);
  bool deleted2 = false;
  Context context;
  context.SetPtr<Item>(unowned_item.get());
  context.SetNew<Item>(&deleted2);
  EXPECT_FALSE(deleted1);
  EXPECT_FALSE(deleted2);
}

TEST(ContextTest, SetOwnedDoesNotDeletePreviousUnownedItems) {
  bool deleted1 = false;
  auto unowned_item = std::make_unique<Item>(&deleted1);
  bool deleted2 = false;
  Context context;
  context.SetPtr<Item>(unowned_item.get());
  context.SetOwned<Item>(std::make_unique<Item>(&deleted2));
  EXPECT_FALSE(deleted1);
  EXPECT_FALSE(deleted2);
}

TEST(ContextTest, SetPtrDoesNotDeletePreviousUnownedItems) {
  bool deleted1 = false;
  auto unowned_item1 = std::make_unique<Item>(&deleted1);
  bool deleted2 = false;
  auto unowned_item2 = std::make_unique<Item>(&deleted2);
  Context context;
  context.SetPtr<Item>(unowned_item1.get());
  context.SetPtr<Item>(unowned_item2.get());
  EXPECT_FALSE(deleted1);
  EXPECT_FALSE(deleted2);
}

}  // namespace 
}  // namespace gb