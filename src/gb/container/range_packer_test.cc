// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/container/range_packer.h"

#include <algorithm>
#include <optional>
#include <random>
#include <vector>

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(RangePackerTest, StartsAsOneGap) {
  RangePacker packer(100);
  EXPECT_EQ(packer.GetCapacity(), 100);
  EXPECT_EQ(packer.GetUsed(), 0);
  EXPECT_EQ(packer.GetGapCount(), 1);
  EXPECT_EQ(packer.GetLargestGap(), 100);
}

TEST(RangePackerTest, PlacesInOrder) {
  RangePacker packer(100);
  EXPECT_EQ(packer.Place(10), 0);
  EXPECT_EQ(packer.Place(20), 10);
  EXPECT_EQ(packer.Place(30), 30);
  EXPECT_EQ(packer.GetUsed(), 60);
  EXPECT_EQ(packer.GetGapCount(), 1);
  EXPECT_EQ(packer.GetLargestGap(), 40);
}

TEST(RangePackerTest, FillsThenFails) {
  RangePacker packer(100);
  EXPECT_EQ(packer.Place(60), 0);
  EXPECT_EQ(packer.Place(50), std::nullopt);
  EXPECT_EQ(packer.Place(40), 60);
  EXPECT_EQ(packer.GetUsed(), 100);
  EXPECT_EQ(packer.GetGapCount(), 0);
  EXPECT_EQ(packer.GetLargestGap(), 0);
  EXPECT_EQ(packer.Place(1), std::nullopt);
}

TEST(RangePackerTest, PicksSmallestGapThatFits) {
  RangePacker packer(100);
  ASSERT_EQ(packer.Place(30), 0);
  ASSERT_EQ(packer.Place(10), 30);
  ASSERT_EQ(packer.Place(10), 40);
  ASSERT_EQ(packer.Place(10), 50);
  ASSERT_EQ(packer.Place(40), 60);

  // Gaps of 30, 10, and 40, with placed ranges between them.
  packer.Remove(0, 30);
  packer.Remove(40, 10);
  packer.Remove(60, 40);
  EXPECT_EQ(packer.GetGapCount(), 3);

  EXPECT_EQ(packer.Place(5), 40);
  EXPECT_EQ(packer.Place(20), 0);
  EXPECT_EQ(packer.Place(35), 60);
}

TEST(RangePackerTest, PicksLowestOffsetOfEqualGaps) {
  RangePacker packer(50);
  ASSERT_EQ(packer.Place(10), 0);
  ASSERT_EQ(packer.Place(10), 10);
  ASSERT_EQ(packer.Place(10), 20);
  ASSERT_EQ(packer.Place(10), 30);
  ASSERT_EQ(packer.Place(10), 40);
  packer.Remove(40, 10);
  packer.Remove(0, 10);
  packer.Remove(20, 10);
  EXPECT_EQ(packer.Place(10), 0);
  EXPECT_EQ(packer.Place(10), 20);
  EXPECT_EQ(packer.Place(10), 40);
}

TEST(RangePackerTest, RemoveMergesWithNextGap) {
  RangePacker packer(30);
  ASSERT_EQ(packer.Place(10), 0);
  ASSERT_EQ(packer.Place(10), 10);
  packer.Remove(10, 10);
  EXPECT_EQ(packer.GetGapCount(), 1);
  EXPECT_EQ(packer.GetLargestGap(), 20);
  EXPECT_EQ(packer.Place(20), 10);
}

TEST(RangePackerTest, RemoveMergesWithPreviousGap) {
  RangePacker packer(30);
  ASSERT_EQ(packer.Place(10), 0);
  ASSERT_EQ(packer.Place(10), 10);
  ASSERT_EQ(packer.Place(10), 20);
  packer.Remove(0, 10);
  packer.Remove(10, 10);
  EXPECT_EQ(packer.GetGapCount(), 1);
  EXPECT_EQ(packer.GetLargestGap(), 20);
  EXPECT_EQ(packer.Place(20), 0);
}

TEST(RangePackerTest, RemoveMergesWithGapsOnBothSides) {
  RangePacker packer(30);
  ASSERT_EQ(packer.Place(10), 0);
  ASSERT_EQ(packer.Place(10), 10);
  ASSERT_EQ(packer.Place(10), 20);
  packer.Remove(0, 10);
  packer.Remove(20, 10);
  EXPECT_EQ(packer.GetGapCount(), 2);
  packer.Remove(10, 10);
  EXPECT_EQ(packer.GetUsed(), 0);
  EXPECT_EQ(packer.GetGapCount(), 1);
  EXPECT_EQ(packer.GetLargestGap(), 30);
}

TEST(RangePackerTest, GrowAddsGap) {
  RangePacker packer(10);
  ASSERT_EQ(packer.Place(10), 0);
  packer.Grow(30);
  EXPECT_EQ(packer.GetCapacity(), 30);
  EXPECT_EQ(packer.GetUsed(), 10);
  EXPECT_EQ(packer.GetGapCount(), 1);
  EXPECT_EQ(packer.GetLargestGap(), 20);
  EXPECT_EQ(packer.Place(20), 10);
}

TEST(RangePackerTest, GrowMergesWithGapAtEnd) {
  RangePacker packer(20);
  ASSERT_EQ(packer.Place(10), 0);
  packer.Grow(30);
  EXPECT_EQ(packer.GetGapCount(), 1);
  EXPECT_EQ(packer.GetLargestGap(), 20);
  EXPECT_EQ(packer.Place(20), 10);
}

// A simple model of the packer: a flag per offset, set if used.
class Model {
 public:
  struct Range {
    int offset;
    int size;
  };

  explicit Model(int capacity) : used_(capacity, 0) {}

  int GetUsed() const { return used_count_; }

  // Returns every maximal run of unused offsets, in order.
  std::vector<Range> GetGaps() const {
    std::vector<Range> gaps;
    const int capacity = static_cast<int>(used_.size());
    for (int i = 0; i < capacity; ++i) {
      if (used_[i]) {
        continue;
      }
      if (!gaps.empty() && gaps.back().offset + gaps.back().size == i) {
        ++gaps.back().size;
      } else {
        gaps.push_back({i, 1});
      }
    }
    return gaps;
  }

  // Marks the range used or unused, returning false if any offset already
  // was.
  bool Set(int offset, int size, bool used) {
    for (int i = offset; i < offset + size; ++i) {
      if ((used_[i] != 0) == used) {
        return false;
      }
      used_[i] = used ? 1 : 0;
    }
    used_count_ += used ? size : -size;
    return true;
  }

  void Grow(int capacity) { used_.resize(capacity, 0); }

 private:
  std::vector<char> used_;
  int used_count_ = 0;
};

// Returns the offset the best fit for `size` would take in `gaps`, if any.
std::optional<int> GetBestFit(const std::vector<Model::Range>& gaps, int size) {
  std::optional<Model::Range> best;
  for (const Model::Range& gap : gaps) {
    if (gap.size >= size && (!best || gap.size < best->size)) {
      best = gap;
    }
  }
  if (!best) {
    return std::nullopt;
  }
  return best->offset;
}

TEST(RangePackerTest, AgreesWithModel) {
  constexpr int kSteps = 2000;
  constexpr int kMaxSize = 40;
  std::mt19937 random(1234);
  RangePacker packer(500);
  Model model(500);
  std::vector<Model::Range> placed;
  std::vector<Model::Range> gaps = model.GetGaps();
  for (int step = 0; step < kSteps; ++step) {
    const int action = std::uniform_int_distribution<int>(0, 99)(random);
    if (action < 55) {
      const int size = std::uniform_int_distribution<int>(1, kMaxSize)(random);
      const std::optional<int> offset = packer.Place(size);
      ASSERT_EQ(offset, GetBestFit(gaps, size))
          << "Step " << step << ", size " << size;
      if (offset) {
        ASSERT_TRUE(model.Set(*offset, size, true)) << "Step " << step;
        placed.push_back({*offset, size});
      }
    } else if (action < 99) {
      if (placed.empty()) {
        continue;
      }
      const int index = std::uniform_int_distribution<int>(
          0, static_cast<int>(placed.size()) - 1)(random);
      const Model::Range range = placed[index];
      placed[index] = placed.back();
      placed.pop_back();
      packer.Remove(range.offset, range.size);
      ASSERT_TRUE(model.Set(range.offset, range.size, false))
          << "Step " << step;
    } else {
      const int capacity = packer.GetCapacity() + 50;
      packer.Grow(capacity);
      model.Grow(capacity);
    }

    gaps = model.GetGaps();
    int largest = 0;
    for (const Model::Range& gap : gaps) {
      largest = std::max(largest, gap.size);
    }
    ASSERT_EQ(packer.GetUsed(), model.GetUsed()) << "Step " << step;
    ASSERT_EQ(packer.GetGapCount(), static_cast<int>(gaps.size()))
        << "Step " << step;
    ASSERT_EQ(packer.GetLargestGap(), largest) << "Step " << step;
  }
}

}  // namespace
}  // namespace gb
