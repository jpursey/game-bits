// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/container/buffer_view_2d.h"

#include "gb/container/buffer_view_test_types.h"
#include "gmock/gmock.h"

namespace gb {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

void ValidateView(const BufferView2d<PosItem<glm::ivec2>>& view) {
  const glm::ivec2& size = view.GetSize();
  const glm::ivec2& origin = view.GetOrigin();
  glm::ivec2 rpos;
  for (rpos.x = 0; rpos.x < size.x; ++rpos.x) {
    for (rpos.y = 0; rpos.y < size.y; ++rpos.y) {
      EXPECT_EQ(view.GetRelative(rpos).pos, origin + rpos)
          << "rpos=" << rpos << ", origin+rpos=" << (origin + rpos);
    }
  }
}

TEST(BufferView2dTest, Construct) {
  ResetOperations<glm::ivec2>();
  BufferView2d<PosItem<glm::ivec2>> view({2, 2});
  EXPECT_EQ(view.GetSize(), glm::ivec2(2, 2));
  EXPECT_EQ(view.GetOrigin(), glm::ivec2(0, 0));
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kConstruct, {0, 0}),
                                   Vec2Op(OpType::kConstruct, {1, 0}),
                                   Vec2Op(OpType::kConstruct, {0, 1}),
                                   Vec2Op(OpType::kConstruct, {1, 1})));
  ValidateView(view);
}

TEST(BufferView2dTest, ConstructAtOffset) {
  ResetOperations<glm::ivec2>();
  BufferView2d<PosItem<glm::ivec2>> view({2, 2}, {4, 7});
  EXPECT_EQ(view.GetSize(), glm::ivec2(2, 2));
  EXPECT_EQ(view.GetOrigin(), glm::ivec2(4, 7));
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kConstruct, {4, 7}),
                                   Vec2Op(OpType::kConstruct, {5, 7}),
                                   Vec2Op(OpType::kConstruct, {4, 8}),
                                   Vec2Op(OpType::kConstruct, {5, 8})));
  ValidateView(view);
}

TEST(BufferView2dTest, Destruct) {
  {
    BufferView2d<PosItem<glm::ivec2>> view({2, 2});
    ResetOperations<glm::ivec2>();
  }
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kDestruct, {0, 0}),
                                   Vec2Op(OpType::kDestruct, {1, 0}),
                                   Vec2Op(OpType::kDestruct, {0, 1}),
                                   Vec2Op(OpType::kDestruct, {1, 1})));
}

void ValidateSetOrigin(BufferView2d<PosItem<glm::ivec2>>& view,
                       const glm::ivec2& delta) {
  ResetOperations<glm::ivec2>();

  const glm::ivec2 origin = view.GetOrigin() + delta;
  view.SetOrigin(origin);
  EXPECT_EQ(view.GetOrigin(), origin);

  ValidateView(view);

  const glm::ivec2 udelta = {std::max(delta.x, -delta.x),
                             std::max(delta.y, -delta.y)};
  const glm::ivec2& size = view.GetSize();
  const glm::ivec2 rsize = size - udelta;
  const size_t op_count = (size.x * size.y) - (rsize.x * rsize.y);
  EXPECT_EQ(GetOperations<glm::ivec2>().size(), op_count);
}

TEST(BufferView2dTest, SetOrigin) {
  const int32_t kSeries2[] = {0, 1, 1, 2, -1, -1, -2};
  const int32_t kSeries4[] = {0, 1, 2, 3, 2, 1, 4, -1, -2, -3, -2, -1, -4};
  const int32_t kSeries8[] = {0,  1,  3,  5,  7,  6,  4,  1, 8,
                              -1, -3, -5, -7, -6, -4, -1, -8};
  {
    BufferView2d<PosItem<glm::ivec2>> view({4, 4}, {15, 25});
    for (int32_t x : kSeries4) {
      for (int32_t y : kSeries4) {
        ValidateSetOrigin(view, {x, y});
      }
    }
  }
  {
    BufferView2d<PosItem<glm::ivec2>> view({2, 4}, {15, 25});
    for (int32_t x : kSeries2) {
      for (int32_t y : kSeries4) {
        ValidateSetOrigin(view, {x, y});
      }
    }
  }
  {
    BufferView2d<PosItem<glm::ivec2>> view({4, 8}, {15, 25});
    for (int32_t x : kSeries4) {
      for (int32_t y : kSeries8) {
        ValidateSetOrigin(view, {x, y});
      }
    }
  }
}

TEST(BufferView2dTest, ClearRelative) {
  BufferView2d<PosItem<glm::ivec2>> view({4, 4}, {14, 24});
  view.SetOrigin({15, 25});

  ResetOperations<glm::ivec2>();
  view.ClearRelative({1, 1}, {1, 1});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {16, 26}, {16, 26})));

  ResetOperations<glm::ivec2>();
  view.ClearRelative({1, 2}, {2, 1});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {16, 27}, {16, 27}),
                                   Vec2Op(OpType::kClear, {17, 27}, {17, 27})));

  ResetOperations<glm::ivec2>();
  view.ClearRelative({2, 1}, {1, 2});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {17, 26}, {17, 26}),
                                   Vec2Op(OpType::kClear, {17, 27}, {17, 27})));
  ResetOperations<glm::ivec2>();
  view.ClearRelative({2, 2}, {2, 2});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {17, 27}, {17, 27}),
                                   Vec2Op(OpType::kClear, {17, 28}, {17, 28}),
                                   Vec2Op(OpType::kClear, {18, 27}, {18, 27}),
                                   Vec2Op(OpType::kClear, {18, 28}, {18, 28})));
}

TEST(BufferView2dTest, Clear) {
  BufferView2d<PosItem<glm::ivec2>> view({4, 4}, {14, 24});
  view.SetOrigin({15, 25});

  ResetOperations<glm::ivec2>();
  view.Clear({16, 26}, {1, 1});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {16, 26}, {16, 26})));

  ResetOperations<glm::ivec2>();
  view.Clear({17, 27}, {2, 2});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {17, 27}, {17, 27}),
                                   Vec2Op(OpType::kClear, {17, 28}, {17, 28}),
                                   Vec2Op(OpType::kClear, {18, 27}, {18, 27}),
                                   Vec2Op(OpType::kClear, {18, 28}, {18, 28})));

  ResetOperations<glm::ivec2>();
  view.Clear({14, 25}, {1, 4});
  EXPECT_THAT(GetOperations<glm::ivec2>(), IsEmpty());

  ResetOperations<glm::ivec2>();
  view.Clear({19, 25}, {1, 4});
  EXPECT_THAT(GetOperations<glm::ivec2>(), IsEmpty());

  ResetOperations<glm::ivec2>();
  view.Clear({15, 24}, {4, 1});
  EXPECT_THAT(GetOperations<glm::ivec2>(), IsEmpty());

  ResetOperations<glm::ivec2>();
  view.Clear({15, 29}, {4, 1});
  EXPECT_THAT(GetOperations<glm::ivec2>(), IsEmpty());

  ResetOperations<glm::ivec2>();
  view.Clear({17, 25}, {3, 1});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {17, 25}, {17, 25}),
                                   Vec2Op(OpType::kClear, {18, 25}, {18, 25})));

  ResetOperations<glm::ivec2>();
  view.Clear({14, 25}, {2, 1});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {15, 25}, {15, 25})));

  ResetOperations<glm::ivec2>();
  view.Clear({14, 25}, {6, 1});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {15, 25}, {15, 25}),
                                   Vec2Op(OpType::kClear, {16, 25}, {16, 25}),
                                   Vec2Op(OpType::kClear, {17, 25}, {17, 25}),
                                   Vec2Op(OpType::kClear, {18, 25}, {18, 25})));

  ResetOperations<glm::ivec2>();
  view.Clear({15, 27}, {1, 3});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {15, 27}, {15, 27}),
                                   Vec2Op(OpType::kClear, {15, 28}, {15, 28})));

  ResetOperations<glm::ivec2>();
  view.Clear({15, 24}, {1, 2});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {15, 25}, {15, 25})));

  ResetOperations<glm::ivec2>();
  view.Clear({15, 24}, {1, 6});
  EXPECT_THAT(GetOperations<glm::ivec2>(),
              UnorderedElementsAre(Vec2Op(OpType::kClear, {15, 25}, {15, 25}),
                                   Vec2Op(OpType::kClear, {15, 26}, {15, 26}),
                                   Vec2Op(OpType::kClear, {15, 27}, {15, 27}),
                                   Vec2Op(OpType::kClear, {15, 28}, {15, 28})));

  ResetOperations<glm::ivec2>();
  view.Clear({14, 24}, {6, 6});
  EXPECT_EQ(GetOperations<glm::ivec2>().size(), 16);
}

TEST(BufferView2dTest, GetNonRelative) {
  BufferView2d<PosItem<glm::ivec2>> view({2, 2}, {14, 24});
  view.SetOrigin({15, 25});

  EXPECT_EQ(view.Get({14, 25}), nullptr);
  EXPECT_EQ(view.Get({15, 24}), nullptr);
  EXPECT_EQ(view.Get({15, 25}), &view.GetRelative({0, 0}));
  EXPECT_EQ(view.Get({15, 26}), &view.GetRelative({0, 1}));
  EXPECT_EQ(view.Get({16, 25}), &view.GetRelative({1, 0}));
  EXPECT_EQ(view.Get({16, 26}), &view.GetRelative({1, 1}));
  EXPECT_EQ(view.Get({17, 26}), nullptr);
  EXPECT_EQ(view.Get({16, 27}), nullptr);

  EXPECT_EQ(view.Modify({14, 25}), nullptr);
  EXPECT_EQ(view.Modify({15, 24}), nullptr);
  EXPECT_EQ(view.Modify({15, 25}), &view.ModifyRelative({0, 0}));
  EXPECT_EQ(view.Modify({15, 26}), &view.ModifyRelative({0, 1}));
  EXPECT_EQ(view.Modify({16, 25}), &view.ModifyRelative({1, 0}));
  EXPECT_EQ(view.Modify({16, 26}), &view.ModifyRelative({1, 1}));
  EXPECT_EQ(view.Modify({17, 26}), nullptr);
  EXPECT_EQ(view.Modify({16, 27}), nullptr);
}

TEST(BufferView2dTest, GetIsModify) {
  BufferView2d<PosItem<glm::ivec2>> view({2, 2}, {14, 24});
  view.SetOrigin({15, 25});

  EXPECT_EQ(view.Get({15, 25}), view.Modify({15, 25}));
  EXPECT_EQ(view.Get({15, 26}), view.Modify({15, 26}));
  EXPECT_EQ(view.Get({16, 25}), view.Modify({16, 25}));
  EXPECT_EQ(view.Get({16, 26}), view.Modify({16, 26}));
}

TEST(BufferView2dTest, Set) {
  BufferView2d<int> view({2, 2}, {14, 24});
  view.SetOrigin({15, 25});

  EXPECT_FALSE(view.Set({14, 25}, 1425));
  EXPECT_FALSE(view.Set({15, 24}, 1524));
  EXPECT_TRUE(view.Set({15, 25}, 1525));
  EXPECT_EQ(view.GetRelative({0, 0}), 1525);
  EXPECT_TRUE(view.Set({15, 26}, 1526));
  EXPECT_EQ(view.GetRelative({0, 1}), 1526);
  EXPECT_TRUE(view.Set({16, 25}, 1625));
  EXPECT_EQ(view.GetRelative({1, 0}), 1625);
  EXPECT_TRUE(view.Set({16, 26}, 1626));
  EXPECT_EQ(view.GetRelative({1, 1}), 1626);
  EXPECT_FALSE(view.Set({17, 26}, 1726));
  EXPECT_FALSE(view.Set({16, 27}, 1627));

  view.SetRelative({0, 0}, 100);
  EXPECT_EQ(view.GetRelative({0, 0}), 100);
  view.SetRelative({0, 1}, 101);
  EXPECT_EQ(view.GetRelative({0, 1}), 101);
  view.SetRelative({1, 0}, 110);
  EXPECT_EQ(view.GetRelative({1, 0}), 110);
  view.SetRelative({1, 1}, 111);
  EXPECT_EQ(view.GetRelative({1, 1}), 111);
}

TEST(BufferView2dTest, DefaultOperationsWork) {
  ResetOperations<int32_t>();
  {
    BufferView2d<DefaultItem> view({2, 2});
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1)));
    view.ModifyRelative({0, 0}).value = 0;
    view.ModifyRelative({0, 1}).value = 1;
    view.ModifyRelative({1, 0}).value = 10;
    view.ModifyRelative({1, 1}).value = 11;

    ResetOperations<int32_t>();
    view.ClearRelative({0, 1}, {1, 1});
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kDestruct, -1),
                                     IntOp(OpType::kClear, -1, 1)));
    EXPECT_EQ(view.GetRelative({0, 1}).value, -1);

    ResetOperations<int32_t>();
    view.ClearRelative({1, 0}, {1, 2});
    EXPECT_EQ(view.GetRelative({1, 0}).value, -1);
    EXPECT_EQ(view.GetRelative({1, 1}).value, -1);
    EXPECT_THAT(
        GetOperations<int32_t>(),
        UnorderedElementsAre(
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1),
            IntOp(OpType::kDestruct, -1), IntOp(OpType::kDestruct, -1),
            IntOp(OpType::kClear, -1, 10), IntOp(OpType::kClear, -1, 11)));
    view.ModifyRelative({0, 1}).value = 1;
    view.ModifyRelative({1, 0}).value = 10;
    view.ModifyRelative({1, 1}).value = 11;
    ResetOperations<int32_t>();
  }
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(
                  IntOp(OpType::kDestruct, 0), IntOp(OpType::kDestruct, 1),
                  IntOp(OpType::kDestruct, 10), IntOp(OpType::kDestruct, 11)));
}

TEST(BufferView2dTest, BasicOverrideOperationsWork) {
  ResetOperations<int32_t>();
  {
    BufferView2d<Item> view({2, 2});
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kConstruct, -1)));
    view.ModifyRelative({0, 0}).value = 0;
    view.ModifyRelative({0, 1}).value = 1;
    view.ModifyRelative({1, 0}).value = 10;
    view.ModifyRelative({1, 1}).value = 11;

    ResetOperations<int32_t>();
    view.ClearRelative({0, 1}, {1, 1});
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kClear, -1, 1)));
    EXPECT_EQ(view.GetRelative({0, 1}).value, -1);

    ResetOperations<int32_t>();
    view.ClearRelative({1, 0}, {1, 2});
    EXPECT_EQ(view.GetRelative({1, 0}).value, -1);
    EXPECT_EQ(view.GetRelative({1, 1}).value, -1);
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kClear, -1, 10),
                                     IntOp(OpType::kClear, -1, 11)));
    ResetOperations<int32_t>();
  }
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(
                  IntOp(OpType::kDestruct, -2), IntOp(OpType::kDestruct, -2),
                  IntOp(OpType::kDestruct, -2), IntOp(OpType::kDestruct, -2)));
}

}  // namespace
}  // namespace gb