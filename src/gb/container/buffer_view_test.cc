// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/container/buffer_view.h"

#include "gb/container/buffer_view_test_types.h"
#include "gmock/gmock.h"

namespace gb {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(BufferViewTest, Construct) {
  ResetOperations<int32_t>();
  BufferView<PosItem<int32_t>> view(4);
  EXPECT_EQ(view.GetSize(), 4);
  EXPECT_EQ(view.GetOrigin(), 0);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(
                  IntOp(OpType::kConstruct, 0), IntOp(OpType::kConstruct, 1),
                  IntOp(OpType::kConstruct, 2), IntOp(OpType::kConstruct, 3)));
  EXPECT_EQ(view.GetRelative(0).pos, 0);
  EXPECT_EQ(view.GetRelative(1).pos, 1);
  EXPECT_EQ(view.GetRelative(2).pos, 2);
  EXPECT_EQ(view.GetRelative(3).pos, 3);
}

TEST(BufferViewTest, ConstructAtOffset) {
  ResetOperations<int32_t>();
  BufferView<PosItem<int32_t>> view(4, 6);
  EXPECT_EQ(view.GetSize(), 4);
  EXPECT_EQ(view.GetOrigin(), 6);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(
                  IntOp(OpType::kConstruct, 6), IntOp(OpType::kConstruct, 7),
                  IntOp(OpType::kConstruct, 8), IntOp(OpType::kConstruct, 9)));
  EXPECT_EQ(view.GetRelative(0).pos, 6);
  EXPECT_EQ(view.GetRelative(1).pos, 7);
  EXPECT_EQ(view.GetRelative(2).pos, 8);
  EXPECT_EQ(view.GetRelative(3).pos, 9);
}

TEST(BufferViewTest, Destruct) {
  {
    BufferView<PosItem<int32_t>> view(4);
    ResetOperations<int32_t>();
  }
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(
                  IntOp(OpType::kDestruct, 0), IntOp(OpType::kDestruct, 1),
                  IntOp(OpType::kDestruct, 2), IntOp(OpType::kDestruct, 3)));
}

TEST(BufferViewTest, SetOrigin) {
  BufferView<PosItem<int32_t>> view(4, 15);

  // Full reset smaller
  ResetOperations<int32_t>();
  view.SetOrigin(11);
  EXPECT_EQ(view.GetOrigin(), 11);
  EXPECT_EQ(view.GetRelative(0).pos, 11);
  EXPECT_EQ(view.GetRelative(1).pos, 12);
  EXPECT_EQ(view.GetRelative(2).pos, 13);
  EXPECT_EQ(view.GetRelative(3).pos, 14);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 11, 15),
                                   IntOp(OpType::kClear, 12, 16),
                                   IntOp(OpType::kClear, 13, 17),
                                   IntOp(OpType::kClear, 14, 18)));

  // Full reset bigger
  ResetOperations<int32_t>();
  view.SetOrigin(15);
  EXPECT_EQ(view.GetOrigin(), 15);
  EXPECT_EQ(view.GetRelative(0).pos, 15);
  EXPECT_EQ(view.GetRelative(1).pos, 16);
  EXPECT_EQ(view.GetRelative(2).pos, 17);
  EXPECT_EQ(view.GetRelative(3).pos, 18);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 15, 11),
                                   IntOp(OpType::kClear, 16, 12),
                                   IntOp(OpType::kClear, 17, 13),
                                   IntOp(OpType::kClear, 18, 14)));

  // Increment smaller
  ResetOperations<int32_t>();
  view.SetOrigin(14);
  EXPECT_EQ(view.GetOrigin(), 14);
  EXPECT_EQ(view.GetRelative(0).pos, 14);
  EXPECT_EQ(view.GetRelative(1).pos, 15);
  EXPECT_EQ(view.GetRelative(2).pos, 16);
  EXPECT_EQ(view.GetRelative(3).pos, 17);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 14, 18)));

  ResetOperations<int32_t>();
  view.SetOrigin(12);
  EXPECT_EQ(view.GetOrigin(), 12);
  EXPECT_EQ(view.GetRelative(0).pos, 12);
  EXPECT_EQ(view.GetRelative(1).pos, 13);
  EXPECT_EQ(view.GetRelative(2).pos, 14);
  EXPECT_EQ(view.GetRelative(3).pos, 15);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 12, 16),
                                   IntOp(OpType::kClear, 13, 17)));

  ResetOperations<int32_t>();
  view.SetOrigin(9);
  EXPECT_EQ(view.GetOrigin(), 9);
  EXPECT_EQ(view.GetRelative(0).pos, 9);
  EXPECT_EQ(view.GetRelative(1).pos, 10);
  EXPECT_EQ(view.GetRelative(2).pos, 11);
  EXPECT_EQ(view.GetRelative(3).pos, 12);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 9, 13),
                                   IntOp(OpType::kClear, 10, 14),
                                   IntOp(OpType::kClear, 11, 15)));

  // Reset
  view.SetOrigin(15);

  // Increment bigger
  ResetOperations<int32_t>();
  view.SetOrigin(16);
  EXPECT_EQ(view.GetRelative(0).pos, 16);
  EXPECT_EQ(view.GetRelative(1).pos, 17);
  EXPECT_EQ(view.GetRelative(2).pos, 18);
  EXPECT_EQ(view.GetRelative(3).pos, 19);
  EXPECT_EQ(view.GetOrigin(), 16);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 19, 15)));

  ResetOperations<int32_t>();
  view.SetOrigin(18);
  EXPECT_EQ(view.GetOrigin(), 18);
  EXPECT_EQ(view.GetRelative(0).pos, 18);
  EXPECT_EQ(view.GetRelative(1).pos, 19);
  EXPECT_EQ(view.GetRelative(2).pos, 20);
  EXPECT_EQ(view.GetRelative(3).pos, 21);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 20, 16),
                                   IntOp(OpType::kClear, 21, 17)));

  ResetOperations<int32_t>();
  view.SetOrigin(21);
  EXPECT_EQ(view.GetOrigin(), 21);
  EXPECT_EQ(view.GetRelative(0).pos, 21);
  EXPECT_EQ(view.GetRelative(1).pos, 22);
  EXPECT_EQ(view.GetRelative(2).pos, 23);
  EXPECT_EQ(view.GetRelative(3).pos, 24);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 22, 18),
                                   IntOp(OpType::kClear, 23, 19),
                                   IntOp(OpType::kClear, 24, 20)));
}

TEST(BufferViewTest, ClearRelative) {
  BufferView<PosItem<int32_t>> view(4, 14);
  view.SetOrigin(15);

  ResetOperations<int32_t>();
  view.ClearRelative(1, 1);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 16, 16)));

  ResetOperations<int32_t>();
  view.ClearRelative(2, 2);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 17, 17),
                                   IntOp(OpType::kClear, 18, 18)));
}

TEST(BufferViewTest, Clear) {
  BufferView<PosItem<int32_t>> view(4, 14);
  view.SetOrigin(15);

  ResetOperations<int32_t>();
  view.Clear(16, 1);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 16, 16)));

  ResetOperations<int32_t>();
  view.Clear(17, 2);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 17, 17),
                                   IntOp(OpType::kClear, 18, 18)));

  ResetOperations<int32_t>();
  view.Clear(14, 1);
  EXPECT_THAT(GetOperations<int32_t>(), IsEmpty());

  ResetOperations<int32_t>();
  view.Clear(19, 1);
  EXPECT_THAT(GetOperations<int32_t>(), IsEmpty());

  ResetOperations<int32_t>();
  view.Clear(17, 3);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 17, 17),
                                   IntOp(OpType::kClear, 18, 18)));

  ResetOperations<int32_t>();
  view.Clear(14, 2);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 15, 15)));

  ResetOperations<int32_t>();
  view.Clear(14, 6);
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(IntOp(OpType::kClear, 15, 15),
                                   IntOp(OpType::kClear, 16, 16),
                                   IntOp(OpType::kClear, 17, 17),
                                   IntOp(OpType::kClear, 18, 18)));
}

TEST(BufferViewTest, GetNonRelative) {
  BufferView<PosItem<int32_t>> view(4, 14);
  view.SetOrigin(15);

  EXPECT_EQ(view.Get(14), nullptr);
  EXPECT_EQ(view.Get(15), &view.GetRelative(0));
  EXPECT_EQ(view.Get(16), &view.GetRelative(1));
  EXPECT_EQ(view.Get(17), &view.GetRelative(2));
  EXPECT_EQ(view.Get(18), &view.GetRelative(3));
  EXPECT_EQ(view.Get(19), nullptr);

  EXPECT_EQ(view.Modify(14), nullptr);
  EXPECT_EQ(view.Modify(15), &view.ModifyRelative(0));
  EXPECT_EQ(view.Modify(16), &view.ModifyRelative(1));
  EXPECT_EQ(view.Modify(17), &view.ModifyRelative(2));
  EXPECT_EQ(view.Modify(18), &view.ModifyRelative(3));
  EXPECT_EQ(view.Modify(19), nullptr);
}

TEST(BufferViewTest, GetIsModify) {
  BufferView<PosItem<int32_t>> view(4, 14);
  view.SetOrigin(15);

  EXPECT_EQ(view.Get(15), view.Modify(15));
  EXPECT_EQ(view.Get(16), view.Modify(16));
  EXPECT_EQ(view.Get(17), view.Modify(17));
  EXPECT_EQ(view.Get(18), view.Modify(18));
}

TEST(BufferViewTest, Set) {
  BufferView<int> view(4, 14);
  view.SetOrigin(15);

  EXPECT_FALSE(view.Set(14, 14));
  EXPECT_TRUE(view.Set(15, 15));
  EXPECT_EQ(view.GetRelative(0), 15);
  EXPECT_TRUE(view.Set(16, 16));
  EXPECT_EQ(view.GetRelative(1), 16);
  EXPECT_TRUE(view.Set(17, 17));
  EXPECT_EQ(view.GetRelative(2), 17);
  EXPECT_TRUE(view.Set(18, 18));
  EXPECT_EQ(view.GetRelative(3), 18);
  EXPECT_FALSE(view.Set(19, 19));

  view.SetRelative(0, 20);
  EXPECT_EQ(view.GetRelative(0), 20);
  view.SetRelative(1, 21);
  EXPECT_EQ(view.GetRelative(1), 21);
  view.SetRelative(2, 22);
  EXPECT_EQ(view.GetRelative(2), 22);
  view.SetRelative(3, 23);
  EXPECT_EQ(view.GetRelative(3), 23);
}

TEST(BufferViewTest, DefaultOperationsWork) {
  ResetOperations<int32_t>();
  {
    BufferView<DefaultItem> view(4);
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1)));
    view.ModifyRelative(0).value = 0;
    view.ModifyRelative(1).value = 1;
    view.ModifyRelative(2).value = 2;
    view.ModifyRelative(3).value = 3;

    ResetOperations<int32_t>();
    view.ClearRelative(1, 1);
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kDestruct, -1),
                                     IntOp(OpType::kClear, -1, 1)));
    EXPECT_EQ(view.GetRelative(1).value, -1);

    ResetOperations<int32_t>();
    view.ClearRelative(2, 2);
    EXPECT_EQ(view.GetRelative(2).value, -1);
    EXPECT_EQ(view.GetRelative(3).value, -1);
    EXPECT_THAT(
        GetOperations<int32_t>(),
        UnorderedElementsAre(
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1),
            IntOp(OpType::kDestruct, -1), IntOp(OpType::kDestruct, -1),
            IntOp(OpType::kClear, -1, 2), IntOp(OpType::kClear, -1, 3)));
    view.ModifyRelative(1).value = 1;
    view.ModifyRelative(2).value = 2;
    view.ModifyRelative(3).value = 3;
    ResetOperations<int32_t>();
  }
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(
                  IntOp(OpType::kDestruct, 0), IntOp(OpType::kDestruct, 1),
                  IntOp(OpType::kDestruct, 2), IntOp(OpType::kDestruct, 3)));
}

TEST(BufferViewTest, BasicOverrideOperationsWork) {
  ResetOperations<int32_t>();
  {
    BufferView<Item> view(4);
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1)));
    view.ModifyRelative(0).value = 0;
    view.ModifyRelative(1).value = 1;
    view.ModifyRelative(2).value = 2;
    view.ModifyRelative(3).value = 3;

    ResetOperations<int32_t>();
    view.ClearRelative(1, 1);
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kClear, -1, 1)));
    EXPECT_EQ(view.GetRelative(1).value, -1);

    ResetOperations<int32_t>();
    view.ClearRelative(2, 2);
    EXPECT_EQ(view.GetRelative(2).value, -1);
    EXPECT_EQ(view.GetRelative(3).value, -1);
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kClear, -1, 2),
                                     IntOp(OpType::kClear, -1, 3)));
    ResetOperations<int32_t>();
  }
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(
                  IntOp(OpType::kDestruct, -2), IntOp(OpType::kDestruct, -2),
                  IntOp(OpType::kDestruct, -2), IntOp(OpType::kDestruct, -2)));
}

}  // namespace gb