// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/container/buffer_view_3d.h"

#include "gb/container/buffer_view_test_types.h"
#include "gmock/gmock.h"

namespace gb {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

void ValidateView(const BufferView3d<PosItem<glm::ivec3>>& view) {
  const glm::ivec3& size = view.GetSize();
  const glm::ivec3& origin = view.GetOrigin();
  glm::ivec3 rpos;
  for (rpos.x = 0; rpos.x < size.x; ++rpos.x) {
    for (rpos.y = 0; rpos.y < size.y; ++rpos.y) {
      for (rpos.z = 0; rpos.z < size.z; ++rpos.z) {
        EXPECT_EQ(view.GetRelative(rpos).pos, origin + rpos)
            << "rpos=" << rpos << ", origin+rpos=" << (origin + rpos);
      }
    }
  }
}

TEST(BufferView3dTest, Construct) {
  ResetOperations<glm::ivec3>();
  BufferView3d<PosItem<glm::ivec3>> view({2, 2, 2});
  EXPECT_EQ(view.GetSize(), glm::ivec3(2, 2, 2));
  EXPECT_EQ(view.GetOrigin(), glm::ivec3(0, 0, 0));
  EXPECT_THAT(GetOperations<glm::ivec3>(),
              UnorderedElementsAre(Vec3Op(OpType::kConstruct, {0, 0, 0}),
                                   Vec3Op(OpType::kConstruct, {1, 0, 0}),
                                   Vec3Op(OpType::kConstruct, {0, 1, 0}),
                                   Vec3Op(OpType::kConstruct, {1, 1, 0}),
                                   Vec3Op(OpType::kConstruct, {0, 0, 1}),
                                   Vec3Op(OpType::kConstruct, {1, 0, 1}),
                                   Vec3Op(OpType::kConstruct, {0, 1, 1}),
                                   Vec3Op(OpType::kConstruct, {1, 1, 1})));
  ValidateView(view);
}

TEST(BufferView3dTest, ConstructAtOffset) {
  ResetOperations<glm::ivec3>();
  BufferView3d<PosItem<glm::ivec3>> view({2, 2, 2}, {4, 7, 10});
  EXPECT_EQ(view.GetSize(), glm::ivec3(2, 2, 2));
  EXPECT_EQ(view.GetOrigin(), glm::ivec3(4, 7, 10));
  EXPECT_THAT(GetOperations<glm::ivec3>(),
              UnorderedElementsAre(Vec3Op(OpType::kConstruct, {4, 7, 10}),
                                   Vec3Op(OpType::kConstruct, {5, 7, 10}),
                                   Vec3Op(OpType::kConstruct, {4, 8, 10}),
                                   Vec3Op(OpType::kConstruct, {5, 8, 10}),
                                   Vec3Op(OpType::kConstruct, {4, 7, 11}),
                                   Vec3Op(OpType::kConstruct, {5, 7, 11}),
                                   Vec3Op(OpType::kConstruct, {4, 8, 11}),
                                   Vec3Op(OpType::kConstruct, {5, 8, 11})));
  ValidateView(view);
}

TEST(BufferView3dTest, Destruct) {
  {
    BufferView3d<PosItem<glm::ivec3>> view({2, 2, 2});
    ResetOperations<glm::ivec3>();
  }
  EXPECT_THAT(GetOperations<glm::ivec3>(),
              UnorderedElementsAre(Vec3Op(OpType::kDestruct, {0, 0, 0}),
                                   Vec3Op(OpType::kDestruct, {1, 0, 0}),
                                   Vec3Op(OpType::kDestruct, {0, 1, 0}),
                                   Vec3Op(OpType::kDestruct, {1, 1, 0}),
                                   Vec3Op(OpType::kDestruct, {0, 0, 1}),
                                   Vec3Op(OpType::kDestruct, {1, 0, 1}),
                                   Vec3Op(OpType::kDestruct, {0, 1, 1}),
                                   Vec3Op(OpType::kDestruct, {1, 1, 1})));
}

void ValidateSetOrigin(BufferView3d<PosItem<glm::ivec3>>& view,
                       const glm::ivec3& delta) {
  ResetOperations<glm::ivec3>();

  const glm::ivec3 origin = view.GetOrigin() + delta;
  view.SetOrigin(origin);
  EXPECT_EQ(view.GetOrigin(), origin);

  ValidateView(view);

  const glm::ivec3 udelta = {std::max(delta.x, -delta.x),
                             std::max(delta.y, -delta.y),
                             std::max(delta.z, -delta.z)};
  const glm::ivec3& size = view.GetSize();
  const glm::ivec3 rsize = size - udelta;
  const size_t op_count =
      (size.x * size.y * size.z) - (rsize.x * rsize.y * rsize.z);
  EXPECT_EQ(GetOperations<glm::ivec3>().size(), op_count);
}

TEST(BufferView3dTest, SetOrigin) {
  const int32_t kSeries2[] = {0, 1, 1, 2, -1, -1, -2};
  const int32_t kSeries4[] = {0, 1, 2, 3, 2, 1, 4, -1, -2, -3, -2, -1, -4};
  const int32_t kSeries8[] = {0,  1,  3,  5,  7,  6,  4,  1, 8,
                              -1, -3, -5, -7, -6, -4, -1, -8};
  {
    BufferView3d<PosItem<glm::ivec3>> view({4, 4, 4}, {15, 25, 35});
    for (int32_t x : kSeries4) {
      for (int32_t y : kSeries4) {
        for (int32_t z : kSeries4) {
          ValidateSetOrigin(view, {x, y, z});
        }
      }
    }
  }
  {
    BufferView3d<PosItem<glm::ivec3>> view({2, 4, 8}, {15, 25, 35});
    for (int32_t x : kSeries2) {
      for (int32_t y : kSeries4) {
        for (int32_t z : kSeries8) {
          ValidateSetOrigin(view, {x, y, z});
        }
      }
    }
  }
  {
    BufferView3d<PosItem<glm::ivec3>> view({4, 8, 2}, {15, 25, 35});
    for (int32_t x : kSeries4) {
      for (int32_t y : kSeries8) {
        for (int32_t z : kSeries2) {
          ValidateSetOrigin(view, {x, y, z});
        }
      }
    }
  }
}

TEST(BufferView3dTest, ClearRelative) {
  BufferView3d<PosItem<glm::ivec3>> view({4, 4, 4}, {14, 24, 34});
  view.SetOrigin({15, 25, 35});

  ResetOperations<glm::ivec3>();
  view.ClearRelative({1, 1, 1}, {1, 1, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {16, 26, 36}, {16, 26, 36})));

  ResetOperations<glm::ivec3>();
  view.ClearRelative({1, 2, 3}, {2, 1, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {16, 27, 38}, {16, 27, 38}),
                           Vec3Op(OpType::kClear, {17, 27, 38}, {17, 27, 38})));

  ResetOperations<glm::ivec3>();
  view.ClearRelative({2, 1, 3}, {1, 2, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {17, 26, 38}, {17, 26, 38}),
                           Vec3Op(OpType::kClear, {17, 27, 38}, {17, 27, 38})));
  ResetOperations<glm::ivec3>();
  view.ClearRelative({2, 2, 3}, {2, 2, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {17, 27, 38}, {17, 27, 38}),
                           Vec3Op(OpType::kClear, {17, 28, 38}, {17, 28, 38}),
                           Vec3Op(OpType::kClear, {18, 27, 38}, {18, 27, 38}),
                           Vec3Op(OpType::kClear, {18, 28, 38}, {18, 28, 38})));

  ResetOperations<glm::ivec3>();
  view.ClearRelative({1, 2, 1}, {2, 1, 2});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {16, 27, 36}, {16, 27, 36}),
                           Vec3Op(OpType::kClear, {17, 27, 36}, {17, 27, 36}),
                           Vec3Op(OpType::kClear, {16, 27, 37}, {16, 27, 37}),
                           Vec3Op(OpType::kClear, {17, 27, 37}, {17, 27, 37})));

  ResetOperations<glm::ivec3>();
  view.ClearRelative({2, 1, 1}, {1, 2, 2});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {17, 26, 36}, {17, 26, 36}),
                           Vec3Op(OpType::kClear, {17, 27, 36}, {17, 27, 36}),
                           Vec3Op(OpType::kClear, {17, 26, 37}, {17, 26, 37}),
                           Vec3Op(OpType::kClear, {17, 27, 37}, {17, 27, 37})));
  ResetOperations<glm::ivec3>();
  view.ClearRelative({2, 2, 1}, {2, 2, 2});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {17, 27, 36}, {17, 27, 36}),
                           Vec3Op(OpType::kClear, {17, 28, 36}, {17, 28, 36}),
                           Vec3Op(OpType::kClear, {18, 27, 36}, {18, 27, 36}),
                           Vec3Op(OpType::kClear, {18, 28, 36}, {18, 28, 36}),
                           Vec3Op(OpType::kClear, {17, 27, 37}, {17, 27, 37}),
                           Vec3Op(OpType::kClear, {17, 28, 37}, {17, 28, 37}),
                           Vec3Op(OpType::kClear, {18, 27, 37}, {18, 27, 37}),
                           Vec3Op(OpType::kClear, {18, 28, 37}, {18, 28, 37})));
}

TEST(BufferView3dTest, Clear) {
  BufferView3d<PosItem<glm::ivec3>> view({4, 4, 4}, {14, 24, 34});
  view.SetOrigin({15, 25, 35});

  ResetOperations<glm::ivec3>();
  view.Clear({16, 26, 36}, {1, 1, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {16, 26, 36}, {16, 26, 36})));

  ResetOperations<glm::ivec3>();
  view.Clear({17, 27, 37}, {2, 2, 2});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {17, 27, 37}, {17, 27, 37}),
                           Vec3Op(OpType::kClear, {17, 28, 37}, {17, 28, 37}),
                           Vec3Op(OpType::kClear, {18, 27, 37}, {18, 27, 37}),
                           Vec3Op(OpType::kClear, {18, 28, 37}, {18, 28, 37}),
                           Vec3Op(OpType::kClear, {17, 27, 38}, {17, 27, 38}),
                           Vec3Op(OpType::kClear, {17, 28, 38}, {17, 28, 38}),
                           Vec3Op(OpType::kClear, {18, 27, 38}, {18, 27, 38}),
                           Vec3Op(OpType::kClear, {18, 28, 38}, {18, 28, 38})));

  ResetOperations<glm::ivec3>();
  view.Clear({14, 25, 35}, {1, 4, 4});
  EXPECT_THAT(GetOperations<glm::ivec3>(), IsEmpty());

  ResetOperations<glm::ivec3>();
  view.Clear({19, 25, 35}, {1, 4, 4});
  EXPECT_THAT(GetOperations<glm::ivec3>(), IsEmpty());

  ResetOperations<glm::ivec3>();
  view.Clear({15, 24, 35}, {4, 1, 4});
  EXPECT_THAT(GetOperations<glm::ivec3>(), IsEmpty());

  ResetOperations<glm::ivec3>();
  view.Clear({15, 29, 35}, {4, 1, 4});
  EXPECT_THAT(GetOperations<glm::ivec3>(), IsEmpty());

  ResetOperations<glm::ivec3>();
  view.Clear({15, 25, 34}, {4, 4, 1});
  EXPECT_THAT(GetOperations<glm::ivec3>(), IsEmpty());

  ResetOperations<glm::ivec3>();
  view.Clear({15, 25, 39}, {4, 4, 1});
  EXPECT_THAT(GetOperations<glm::ivec3>(), IsEmpty());

  ResetOperations<glm::ivec3>();
  view.Clear({17, 25, 35}, {3, 1, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {17, 25, 35}, {17, 25, 35}),
                           Vec3Op(OpType::kClear, {18, 25, 35}, {18, 25, 35})));

  ResetOperations<glm::ivec3>();
  view.Clear({14, 25, 35}, {2, 1, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {15, 25, 35}, {15, 25, 35})));

  ResetOperations<glm::ivec3>();
  view.Clear({14, 25, 35}, {6, 1, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {15, 25, 35}, {15, 25, 35}),
                           Vec3Op(OpType::kClear, {16, 25, 35}, {16, 25, 35}),
                           Vec3Op(OpType::kClear, {17, 25, 35}, {17, 25, 35}),
                           Vec3Op(OpType::kClear, {18, 25, 35}, {18, 25, 35})));

  ResetOperations<glm::ivec3>();
  view.Clear({15, 27, 35}, {1, 3, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {15, 27, 35}, {15, 27, 35}),
                           Vec3Op(OpType::kClear, {15, 28, 35}, {15, 28, 35})));

  ResetOperations<glm::ivec3>();
  view.Clear({15, 24, 35}, {1, 2, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {15, 25, 35}, {15, 25, 35})));

  ResetOperations<glm::ivec3>();
  view.Clear({15, 24, 35}, {1, 6, 1});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {15, 25, 35}, {15, 25, 35}),
                           Vec3Op(OpType::kClear, {15, 26, 35}, {15, 26, 35}),
                           Vec3Op(OpType::kClear, {15, 27, 35}, {15, 27, 35}),
                           Vec3Op(OpType::kClear, {15, 28, 35}, {15, 28, 35})));

  ResetOperations<glm::ivec3>();
  view.Clear({15, 25, 37}, {1, 1, 3});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {15, 25, 37}, {15, 25, 37}),
                           Vec3Op(OpType::kClear, {15, 25, 38}, {15, 25, 38})));

  ResetOperations<glm::ivec3>();
  view.Clear({15, 25, 34}, {1, 1, 2});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {15, 25, 35}, {15, 25, 35})));

  ResetOperations<glm::ivec3>();
  view.Clear({15, 25, 34}, {1, 1, 6});
  EXPECT_THAT(
      GetOperations<glm::ivec3>(),
      UnorderedElementsAre(Vec3Op(OpType::kClear, {15, 25, 35}, {15, 25, 35}),
                           Vec3Op(OpType::kClear, {15, 25, 36}, {15, 25, 36}),
                           Vec3Op(OpType::kClear, {15, 25, 37}, {15, 25, 37}),
                           Vec3Op(OpType::kClear, {15, 25, 38}, {15, 25, 38})));

  ResetOperations<glm::ivec3>();
  view.Clear({14, 24, 34}, {6, 6, 6});
  EXPECT_EQ(GetOperations<glm::ivec3>().size(), 64);
}

TEST(BufferView3dTest, GetNonRelative) {
  BufferView3d<PosItem<glm::ivec3>> view({2, 2, 2}, {14, 24, 34});
  view.SetOrigin({15, 25, 35});

  EXPECT_EQ(view.Get({14, 25, 35}), nullptr);
  EXPECT_EQ(view.Get({15, 24, 35}), nullptr);
  EXPECT_EQ(view.Get({15, 25, 34}), nullptr);
  EXPECT_EQ(view.Get({15, 25, 35}), &view.GetRelative({0, 0, 0}));
  EXPECT_EQ(view.Get({15, 26, 35}), &view.GetRelative({0, 1, 0}));
  EXPECT_EQ(view.Get({16, 25, 35}), &view.GetRelative({1, 0, 0}));
  EXPECT_EQ(view.Get({16, 26, 35}), &view.GetRelative({1, 1, 0}));
  EXPECT_EQ(view.Get({15, 25, 36}), &view.GetRelative({0, 0, 1}));
  EXPECT_EQ(view.Get({15, 26, 36}), &view.GetRelative({0, 1, 1}));
  EXPECT_EQ(view.Get({16, 25, 36}), &view.GetRelative({1, 0, 1}));
  EXPECT_EQ(view.Get({16, 26, 36}), &view.GetRelative({1, 1, 1}));
  EXPECT_EQ(view.Get({17, 26, 36}), nullptr);
  EXPECT_EQ(view.Get({16, 27, 36}), nullptr);
  EXPECT_EQ(view.Get({16, 26, 37}), nullptr);

  EXPECT_EQ(view.Modify({14, 25, 35}), nullptr);
  EXPECT_EQ(view.Modify({15, 24, 35}), nullptr);
  EXPECT_EQ(view.Modify({15, 25, 34}), nullptr);
  EXPECT_EQ(view.Modify({15, 25, 35}), &view.ModifyRelative({0, 0, 0}));
  EXPECT_EQ(view.Modify({15, 26, 35}), &view.ModifyRelative({0, 1, 0}));
  EXPECT_EQ(view.Modify({16, 25, 35}), &view.ModifyRelative({1, 0, 0}));
  EXPECT_EQ(view.Modify({16, 26, 35}), &view.ModifyRelative({1, 1, 0}));
  EXPECT_EQ(view.Modify({15, 25, 36}), &view.ModifyRelative({0, 0, 1}));
  EXPECT_EQ(view.Modify({15, 26, 36}), &view.ModifyRelative({0, 1, 1}));
  EXPECT_EQ(view.Modify({16, 25, 36}), &view.ModifyRelative({1, 0, 1}));
  EXPECT_EQ(view.Modify({16, 26, 36}), &view.ModifyRelative({1, 1, 1}));
  EXPECT_EQ(view.Modify({17, 26, 36}), nullptr);
  EXPECT_EQ(view.Modify({16, 27, 36}), nullptr);
  EXPECT_EQ(view.Modify({16, 26, 37}), nullptr);
}

TEST(BufferView3dTest, GetIsModify) {
  BufferView3d<PosItem<glm::ivec3>> view({2, 2, 2}, {14, 24, 34});
  view.SetOrigin({15, 25, 35});

  EXPECT_EQ(view.Get({15, 25, 35}), view.Modify({15, 25, 35}));
  EXPECT_EQ(view.Get({15, 26, 35}), view.Modify({15, 26, 35}));
  EXPECT_EQ(view.Get({16, 25, 35}), view.Modify({16, 25, 35}));
  EXPECT_EQ(view.Get({16, 26, 35}), view.Modify({16, 26, 35}));
  EXPECT_EQ(view.Get({15, 25, 36}), view.Modify({15, 25, 36}));
  EXPECT_EQ(view.Get({15, 26, 36}), view.Modify({15, 26, 36}));
  EXPECT_EQ(view.Get({16, 25, 36}), view.Modify({16, 25, 36}));
  EXPECT_EQ(view.Get({16, 26, 36}), view.Modify({16, 26, 36}));
}

TEST(BufferView3dTest, Set) {
  BufferView3d<int> view({2, 2, 2}, {14, 24, 34});
  view.SetOrigin({15, 25, 35});

  EXPECT_FALSE(view.Set({14, 25, 35}, 142535));
  EXPECT_FALSE(view.Set({15, 24, 35}, 152435));
  EXPECT_FALSE(view.Set({15, 25, 34}, 152534));
  EXPECT_TRUE(view.Set({15, 25, 35}, 152535));
  EXPECT_EQ(view.GetRelative({0, 0, 0}), 152535);
  EXPECT_TRUE(view.Set({15, 26, 35}, 152635));
  EXPECT_EQ(view.GetRelative({0, 1, 0}), 152635);
  EXPECT_TRUE(view.Set({16, 25, 35}, 162535));
  EXPECT_EQ(view.GetRelative({1, 0, 0}), 162535);
  EXPECT_TRUE(view.Set({16, 26, 35}, 162635));
  EXPECT_EQ(view.GetRelative({1, 1, 0}), 162635);
  EXPECT_TRUE(view.Set({15, 25, 36}, 152536));
  EXPECT_EQ(view.GetRelative({0, 0, 1}), 152536);
  EXPECT_TRUE(view.Set({15, 26, 36}, 152636));
  EXPECT_EQ(view.GetRelative({0, 1, 1}), 152636);
  EXPECT_TRUE(view.Set({16, 25, 36}, 162536));
  EXPECT_EQ(view.GetRelative({1, 0, 1}), 162536);
  EXPECT_TRUE(view.Set({16, 26, 36}, 162636));
  EXPECT_EQ(view.GetRelative({1, 1, 1}), 162636);
  EXPECT_FALSE(view.Set({17, 26, 36}, 172636));
  EXPECT_FALSE(view.Set({16, 27, 36}, 162736));
  EXPECT_FALSE(view.Set({16, 26, 37}, 162637));

  view.SetRelative({0, 0, 0}, 1000);
  EXPECT_EQ(view.GetRelative({0, 0, 0}), 1000);
  view.SetRelative({0, 1, 0}, 1010);
  EXPECT_EQ(view.GetRelative({0, 1, 0}), 1010);
  view.SetRelative({1, 0, 0}, 1100);
  EXPECT_EQ(view.GetRelative({1, 0, 0}), 1100);
  view.SetRelative({1, 1, 0}, 1110);
  EXPECT_EQ(view.GetRelative({1, 1, 0}), 1110);
  view.SetRelative({0, 0, 1}, 1001);
  EXPECT_EQ(view.GetRelative({0, 0, 1}), 1001);
  view.SetRelative({0, 1, 1}, 1011);
  EXPECT_EQ(view.GetRelative({0, 1, 1}), 1011);
  view.SetRelative({1, 0, 1}, 1101);
  EXPECT_EQ(view.GetRelative({1, 0, 1}), 1101);
  view.SetRelative({1, 1, 1}, 1111);
  EXPECT_EQ(view.GetRelative({1, 1, 1}), 1111);
}

TEST(BufferView3dTest, DefaultOperationsWork) {
  ResetOperations<int32_t>();
  {
    BufferView3d<DefaultItem> view({2, 2, 2});
    EXPECT_THAT(
        GetOperations<int32_t>(),
        UnorderedElementsAre(
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1),
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1),
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1),
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1)));
    view.ModifyRelative({0, 0, 0}).value = 0;
    view.ModifyRelative({0, 1, 0}).value = 10;
    view.ModifyRelative({1, 0, 0}).value = 100;
    view.ModifyRelative({1, 1, 0}).value = 110;
    view.ModifyRelative({0, 0, 1}).value = 1;
    view.ModifyRelative({0, 1, 1}).value = 11;
    view.ModifyRelative({1, 0, 1}).value = 101;
    view.ModifyRelative({1, 1, 1}).value = 111;

    ResetOperations<int32_t>();
    view.ClearRelative({0, 1, 0}, {1, 1, 1});
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kConstruct, -1),
                                     IntOp(OpType::kDestruct, -1),
                                     IntOp(OpType::kClear, -1, 10)));
    EXPECT_EQ(view.GetRelative({0, 1, 0}).value, -1);

    ResetOperations<int32_t>();
    view.ClearRelative({1, 0, 0}, {1, 2, 1});
    EXPECT_EQ(view.GetRelative({1, 0, 0}).value, -1);
    EXPECT_EQ(view.GetRelative({1, 1, 0}).value, -1);
    EXPECT_THAT(
        GetOperations<int32_t>(),
        UnorderedElementsAre(
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1),
            IntOp(OpType::kDestruct, -1), IntOp(OpType::kDestruct, -1),
            IntOp(OpType::kClear, -1, 100), IntOp(OpType::kClear, -1, 110)));
    view.ModifyRelative({0, 1, 0}).value = 10;
    view.ModifyRelative({1, 0, 0}).value = 100;
    view.ModifyRelative({1, 1, 0}).value = 110;
    ResetOperations<int32_t>();
  }
  EXPECT_THAT(
      GetOperations<int32_t>(),
      UnorderedElementsAre(
          IntOp(OpType::kDestruct, 0), IntOp(OpType::kDestruct, 10),
          IntOp(OpType::kDestruct, 100), IntOp(OpType::kDestruct, 110),
          IntOp(OpType::kDestruct, 1), IntOp(OpType::kDestruct, 11),
          IntOp(OpType::kDestruct, 101), IntOp(OpType::kDestruct, 111)));
}

TEST(BufferView3dTest, BasicOverrideOperationsWork) {
  ResetOperations<int32_t>();
  {
    BufferView3d<Item> view({2, 2, 2});
    EXPECT_THAT(
        GetOperations<int32_t>(),
        UnorderedElementsAre(
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1),
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1),
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1),
            IntOp(OpType::kConstruct, -1), IntOp(OpType::kConstruct, -1)));
    view.ModifyRelative({0, 0, 0}).value = 0;
    view.ModifyRelative({0, 1, 0}).value = 10;
    view.ModifyRelative({1, 0, 0}).value = 100;
    view.ModifyRelative({1, 1, 0}).value = 110;
    view.ModifyRelative({0, 0, 1}).value = 1;
    view.ModifyRelative({0, 1, 1}).value = 11;
    view.ModifyRelative({1, 0, 1}).value = 101;
    view.ModifyRelative({1, 1, 1}).value = 111;

    ResetOperations<int32_t>();
    view.ClearRelative({0, 1, 0}, {1, 1, 1});
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kClear, -1, 10)));

    ResetOperations<int32_t>();
    view.ClearRelative({1, 0, 0}, {1, 2, 1});
    EXPECT_THAT(GetOperations<int32_t>(),
                UnorderedElementsAre(IntOp(OpType::kClear, -1, 100),
                                     IntOp(OpType::kClear, -1, 110)));
    ResetOperations<int32_t>();
  }
  EXPECT_THAT(GetOperations<int32_t>(),
              UnorderedElementsAre(
                  IntOp(OpType::kDestruct, -2), IntOp(OpType::kDestruct, -2),
                  IntOp(OpType::kDestruct, -2), IntOp(OpType::kDestruct, -2),
                  IntOp(OpType::kDestruct, -2), IntOp(OpType::kDestruct, -2),
                  IntOp(OpType::kDestruct, -2), IntOp(OpType::kDestruct, -2)));
}

}  // namespace
}  // namespace gb