// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/container/queue.h"

#include <deque>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

struct Counts {
  Counts() = default;
  int init_construct = 0;
  int copy_construct = 0;
  int move_construct = 0;
  int destruct = 0;
};

class Item {
 public:
  Item(Counts* counts, int value) : counts_(counts), value_(value) {
    ++counts_->init_construct;
  }
  Item(const Item& other) : counts_(other.counts_), value_(other.value_) {
    ++counts_->copy_construct;
  }
  Item(Item&& other)
      : counts_(other.counts_), value_(std::exchange(other.value_, 0)) {
    ++counts_->move_construct;
  }
  Item& operator=(const Item&) = delete;
  Item& operator=(Item&&) = delete;
  ~Item() {
    ++counts_->destruct;
    counts_ = nullptr;
    value_ = -1;
  }

  int GetValue() const { return value_; }

 private:
  Counts* counts_;
  int value_;
};

std::unique_ptr<Queue<Item>> InitQueueImpl(Counts* counts, int start_index) {
  // This creates an "interesting" queue where the there is a skip buffer.
  auto queue = std::make_unique<Queue<Item>>(2);
  queue->emplace(counts, start_index - 1);
  queue->emplace(counts, start_index);
  queue->emplace(counts, start_index + 1);
  queue->pop();
  queue->emplace(counts, start_index + 2);
  queue->emplace(counts, start_index + 3);
  queue->emplace(counts, start_index + 4);
  *counts = {};
  return queue;
}

std::unique_ptr<Queue<Item>> InitQueueWith12345(Counts* counts) {
  return InitQueueImpl(counts, 1);
}

std::unique_ptr<Queue<Item>> InitQueueWith67890(Counts* counts) {
  return InitQueueImpl(counts, 6);
}

TEST(QueueTest, ConstructWithZeroCapacity) {
  Queue<Item> queue(0);
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0);
  EXPECT_EQ(queue.capacity(), 0);
  EXPECT_EQ(queue.grow_capacity(), 0);
}

TEST(QueueTest, ConstructWithNonZeroCapacity) {
  Queue<Item> queue(1);
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0);
  EXPECT_EQ(queue.capacity(), 1);
  EXPECT_EQ(queue.grow_capacity(), 1);
}

TEST(QueueTest, EmplaceWithInitCapacityOfOne) {
  Queue<Item> queue(1, 0);
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0);
  EXPECT_EQ(queue.capacity(), 1);
  EXPECT_EQ(queue.grow_capacity(), 0);

  Counts counts;
  Item& item = queue.emplace(&counts, 1);

  EXPECT_FALSE(queue.empty());
  EXPECT_EQ(queue.size(), 1);
  EXPECT_EQ(queue.capacity(), 1);
  EXPECT_EQ(queue.grow_capacity(), 0);

  EXPECT_EQ(&item, &queue.front());
  EXPECT_EQ(item.GetValue(), 1);

  EXPECT_EQ(counts.init_construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 0);
}

TEST(QueueTest, EmplaceWithGrowCapacityOfOne) {
  Queue<Item> queue(0, 1);
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0);
  EXPECT_EQ(queue.capacity(), 0);
  EXPECT_EQ(queue.grow_capacity(), 1);

  Counts counts;
  Item& item = queue.emplace(&counts, 1);

  EXPECT_FALSE(queue.empty());
  EXPECT_EQ(queue.size(), 1);
  EXPECT_EQ(queue.capacity(), 1);
  EXPECT_EQ(queue.grow_capacity(), 1);

  EXPECT_EQ(&item, &queue.front());
  EXPECT_EQ(item.GetValue(), 1);

  EXPECT_EQ(counts.init_construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 0);
}

TEST(QueueTest, EmplaceTwiceWithGrowCapacityOfOne) {
  Queue<Item> queue(0, 1);
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0);
  EXPECT_EQ(queue.capacity(), 0);
  EXPECT_EQ(queue.grow_capacity(), 1);

  Counts counts;
  Item& item_1 = queue.emplace(&counts, 1);
  Item& item_2 = queue.emplace(&counts, 2);

  EXPECT_FALSE(queue.empty());
  EXPECT_EQ(queue.size(), 2);
  EXPECT_EQ(queue.capacity(), 2);
  EXPECT_EQ(queue.grow_capacity(), 1);

  EXPECT_EQ(&item_1, &queue.front());
  EXPECT_EQ(item_1.GetValue(), 1);
  EXPECT_EQ(item_2.GetValue(), 2);

  EXPECT_EQ(counts.init_construct, 2);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 0);
}

TEST(QueueTest, PopToEmpty) {
  Queue<Item> queue(2);

  Counts counts;
  queue.emplace(&counts, 1);
  queue.pop();

  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0);

  EXPECT_EQ(counts.init_construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 1);
}

TEST(QueueTest, PushByCopy) {
  Queue<Item> queue(2);

  Counts counts;
  Item item(&counts, 1);
  queue.push(item);

  EXPECT_FALSE(queue.empty());
  EXPECT_EQ(queue.size(), 1);
  EXPECT_EQ(queue.front().GetValue(), 1);

  EXPECT_EQ(counts.init_construct, 1);
  EXPECT_EQ(counts.copy_construct, 1);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 0);
}

TEST(QueueTest, PushByMove) {
  Queue<Item> queue(2);

  Counts counts;
  Item item(&counts, 1);
  queue.push(std::move(item));

  EXPECT_FALSE(queue.empty());
  EXPECT_EQ(queue.size(), 1);
  EXPECT_EQ(queue.front().GetValue(), 1);

  EXPECT_EQ(counts.init_construct, 1);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 1);
  EXPECT_EQ(counts.destruct, 0);
}

TEST(QueueTest, FixedSizeQueue) {
  Queue<Item> queue(4, 0);

  Counts counts;
  queue.emplace(&counts, 1);
  queue.emplace(&counts, 2);
  queue.emplace(&counts, 3);
  for (int i = 4; i < 100; ++i) {
    queue.emplace(&counts, i);
    queue.pop();
  }

  EXPECT_EQ(queue.size(), 3);
  EXPECT_EQ(queue.capacity(), 4);
  EXPECT_EQ(queue.front().GetValue(), 97);
  EXPECT_EQ(queue.back().GetValue(), 99);

  EXPECT_EQ(counts.init_construct, 99);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 96);
}

TEST(QueueTest, GrowingQueue) {
  Queue<Item> queue(10);

  Counts counts;
  for (int i = 1; i < 100; ++i) {
    queue.emplace(&counts, i);
    if (i % 3 == 0) {
      queue.pop();
    }
  }

  EXPECT_EQ(queue.size(), 66);
  EXPECT_EQ(queue.capacity(), 80);
  EXPECT_EQ(queue.front().GetValue(), 34);
  EXPECT_EQ(queue.back().GetValue(), 99);

  EXPECT_EQ(counts.init_construct, 99);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 33);
}

TEST(QueueTest, BackEdgeCases) {
  Queue<Item> queue(2);

  Counts counts;
  queue.emplace(&counts, 1);
  EXPECT_EQ(queue.back().GetValue(), 1);

  queue.emplace(&counts, 2);
  queue.emplace(&counts, 3);
  EXPECT_EQ(queue.back().GetValue(), 3);

  queue.pop();
  queue.emplace(&counts, 4);
  queue.emplace(&counts, 5);
  queue.emplace(&counts, 6);
  EXPECT_EQ(queue.back().GetValue(), 6);
}

TEST(QueueTest, Destruct) {
  Counts counts;
  auto queue = InitQueueWith12345(&counts);

  queue.reset();
  EXPECT_EQ(counts.init_construct, 0);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 5);
}

TEST(QueueTest, CopyConstruct) {
  Counts counts;
  auto queue = InitQueueWith12345(&counts);

  Queue<Item> new_queue(*queue);
  EXPECT_EQ(counts.init_construct, 0);
  EXPECT_EQ(counts.copy_construct, 5);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 0);

  EXPECT_EQ(new_queue.capacity(), 5);
  EXPECT_EQ(new_queue.size(), 5);
  EXPECT_EQ(new_queue.front().GetValue(), 1);
  EXPECT_EQ(new_queue.back().GetValue(), 5);
  EXPECT_EQ(new_queue.grow_capacity(), queue->grow_capacity());

  EXPECT_EQ(queue->size(), 5);
  EXPECT_EQ(queue->front().GetValue(), 1);
  EXPECT_EQ(queue->back().GetValue(), 5);
  EXPECT_NE(&queue->front(), &new_queue.front());

  for (int i = 1; i < 6; ++i) {
    EXPECT_EQ(new_queue.front().GetValue(), i);
    EXPECT_EQ(queue->front().GetValue(), i);
    new_queue.pop();
    queue->pop();
  }
}

TEST(QueueTest, MoveConstruct) {
  Counts counts;
  auto queue = InitQueueWith12345(&counts);

  Queue<Item> new_queue(std::move(*queue));
  EXPECT_EQ(counts.init_construct, 0);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 0);

  EXPECT_EQ(new_queue.size(), 5);
  EXPECT_EQ(new_queue.front().GetValue(), 1);
  EXPECT_EQ(new_queue.back().GetValue(), 5);
  EXPECT_EQ(new_queue.grow_capacity(), queue->grow_capacity());

  EXPECT_EQ(queue->size(), 0);

  for (int i = 1; i < 6; ++i) {
    EXPECT_EQ(new_queue.front().GetValue(), i);
    new_queue.pop();
  }
}

TEST(QueueTest, SelfCopyAssignment) {
  Counts counts;
  auto queue = InitQueueWith12345(&counts);
  *queue = *queue;
  EXPECT_EQ(counts.init_construct, 0);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 0);

  EXPECT_EQ(queue->size(), 5);
  EXPECT_EQ(queue->front().GetValue(), 1);
  EXPECT_EQ(queue->back().GetValue(), 5);

  for (int i = 1; i < 6; ++i) {
    EXPECT_EQ(queue->front().GetValue(), i);
    queue->pop();
  }
}

TEST(QueueTest, CopyAssignment) {
  Counts counts;
  auto queue_1 = InitQueueWith12345(&counts);
  auto queue_2 = InitQueueWith67890(&counts);
  *queue_2 = *queue_1;
  EXPECT_EQ(counts.init_construct, 0);
  EXPECT_EQ(counts.copy_construct, 5);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 5);

  EXPECT_EQ(queue_2->size(), 5);
  EXPECT_EQ(queue_2->front().GetValue(), 1);
  EXPECT_EQ(queue_2->back().GetValue(), 5);
  EXPECT_EQ(queue_2->grow_capacity(), queue_1->grow_capacity());

  EXPECT_EQ(queue_1->size(), 5);
  EXPECT_EQ(queue_1->front().GetValue(), 1);
  EXPECT_EQ(queue_1->back().GetValue(), 5);
  EXPECT_NE(&queue_1->front(), &queue_2->front());

  for (int i = 1; i < 6; ++i) {
    EXPECT_EQ(queue_2->front().GetValue(), i);
    EXPECT_EQ(queue_1->front().GetValue(), i);
    queue_2->pop();
    queue_1->pop();
  }
}

TEST(QueueTest, SelfMoveAssignment) {
  Counts counts;
  auto queue = InitQueueWith12345(&counts);
  *queue = std::move(*queue);
  EXPECT_EQ(counts.init_construct, 0);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 0);

  EXPECT_EQ(queue->size(), 5);
  EXPECT_EQ(queue->front().GetValue(), 1);
  EXPECT_EQ(queue->back().GetValue(), 5);

  for (int i = 1; i < 6; ++i) {
    EXPECT_EQ(queue->front().GetValue(), i);
    queue->pop();
  }
}

TEST(QueueTest, MoveAssignment) {
  Counts counts;
  auto queue_1 = InitQueueWith12345(&counts);
  auto queue_2 = InitQueueWith67890(&counts);
  *queue_2 = std::move(*queue_1);
  EXPECT_EQ(counts.init_construct, 0);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 5);

  EXPECT_EQ(queue_2->size(), 5);
  EXPECT_EQ(queue_2->front().GetValue(), 1);
  EXPECT_EQ(queue_2->back().GetValue(), 5);
  EXPECT_EQ(queue_2->grow_capacity(), queue_1->grow_capacity());

  EXPECT_EQ(queue_1->size(), 0);

  for (int i = 1; i < 6; ++i) {
    EXPECT_EQ(queue_2->front().GetValue(), i);
    queue_2->pop();
  }
}

TEST(QueueTest, SwapMethod) {
  Counts counts;
  auto queue_1 = InitQueueWith12345(&counts);
  auto queue_2 = InitQueueWith67890(&counts);

  queue_2->swap(*queue_1);
  EXPECT_EQ(counts.init_construct, 0);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 0);

  EXPECT_EQ(queue_2->size(), 5);
  EXPECT_EQ(queue_2->front().GetValue(), 1);
  EXPECT_EQ(queue_2->back().GetValue(), 5);
  EXPECT_EQ(queue_2->grow_capacity(), queue_1->grow_capacity());

  EXPECT_EQ(queue_1->size(), 5);
  EXPECT_EQ(queue_1->front().GetValue(), 6);
  EXPECT_EQ(queue_1->back().GetValue(), 10);
  EXPECT_NE(&queue_1->front(), &queue_2->front());

  for (int i = 1; i < 6; ++i) {
    EXPECT_EQ(queue_2->front().GetValue(), i);
    EXPECT_EQ(queue_1->front().GetValue(), i + 5);
    queue_2->pop();
    queue_1->pop();
  }
}

TEST(QueueTest, SwapFunction) {
  Counts counts;
  auto queue_1 = InitQueueWith12345(&counts);
  auto queue_2 = InitQueueWith67890(&counts);

  using std::swap;
  swap(*queue_2, *queue_1);
  EXPECT_EQ(counts.init_construct, 0);
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.move_construct, 0);
  EXPECT_EQ(counts.destruct, 0);

  EXPECT_EQ(queue_2->size(), 5);
  EXPECT_EQ(queue_2->front().GetValue(), 1);
  EXPECT_EQ(queue_2->back().GetValue(), 5);
  EXPECT_EQ(queue_2->grow_capacity(), queue_1->grow_capacity());

  EXPECT_EQ(queue_1->size(), 5);
  EXPECT_EQ(queue_1->front().GetValue(), 6);
  EXPECT_EQ(queue_1->back().GetValue(), 10);
  EXPECT_NE(&queue_1->front(), &queue_2->front());

  for (int i = 1; i < 6; ++i) {
    EXPECT_EQ(queue_2->front().GetValue(), i);
    EXPECT_EQ(queue_1->front().GetValue(), i + 5);
    queue_2->pop();
    queue_1->pop();
  }
}

TEST(QueueTest, CopyKeepsCapacity) {
  Queue<int> queue(10);
  queue.push(1);

  Queue<int> copy(queue);
  EXPECT_EQ(copy.capacity(), 10);

  Queue<int> assigned(2);
  assigned = queue;
  EXPECT_EQ(assigned.capacity(), 10);

  Queue<int> copy_of_copy(copy);
  EXPECT_EQ(copy_of_copy.capacity(), 10);
}

TEST(QueueTest, CopyAssignmentTakesGrowCapacity) {
  Queue<int> queue(1, 5);
  Queue<int> assigned(1, 0);
  assigned = queue;
  EXPECT_EQ(assigned.grow_capacity(), 5);
}

TEST(QueueTest, MoveAssignmentTakesGrowCapacity) {
  Queue<int> queue(1, 5);
  Queue<int> assigned(1, 0);
  assigned = std::move(queue);
  EXPECT_EQ(assigned.grow_capacity(), 5);
}

TEST(QueueTest, SwapExchangesGrowCapacity) {
  Queue<int> queue_1(1, 5);
  Queue<int> queue_2(1, 0);
  queue_1.swap(queue_2);
  EXPECT_EQ(queue_1.grow_capacity(), 0);
  EXPECT_EQ(queue_2.grow_capacity(), 5);
}

TEST(QueueTest, VectorOfQueuesMovesQueues) {
  Counts counts;
  std::vector<Queue<Item>> queues;
  for (int i = 0; i < 10; ++i) {
    queues.emplace_back(2);
    queues.back().emplace(&counts, i);
  }
  EXPECT_EQ(counts.copy_construct, 0);
  EXPECT_EQ(counts.destruct, 0);
}

TEST(QueueDeathTest, PushToFullQueueThatCannotGrow) {
  Queue<int> queue(1, 0);
  queue.push(1);
  EXPECT_DEATH(queue.push(2), "full");
}

TEST(QueueDeathTest, PushToZeroCapacityQueueThatCannotGrow) {
  Queue<int> queue(0, 0);
  EXPECT_DEATH(queue.push(1), "full");
}

// Checks queues against std::deque through long runs of pushes and pops, so
// they fill, grow, wrap, and split buckets in many different ways.
TEST(QueueTest, MatchesDeque) {
  const std::pair<int, int> kCapacities[] = {
      {1, 1}, {2, 2}, {3, 1}, {4, 2}, {0, 3}, {5, 7}, {4, 0}, {16, 0},
  };
  for (const auto& [init_capacity, grow_capacity] : kCapacities) {
    SCOPED_TRACE(absl::StrCat("init_capacity=", init_capacity,
                              ", grow_capacity=", grow_capacity));
    Counts counts;
    {
      Queue<Item> queue(init_capacity, grow_capacity);
      std::deque<int> expected;
      std::mt19937 random(init_capacity * 100 + grow_capacity);
      int next_value = 0;
      for (int i = 0; i < 2000; ++i) {
        // Alternate between mostly pushing and mostly popping.
        const int push_chance = (i / 100) % 2 == 0 ? 3 : 1;
        const bool full = grow_capacity == 0 &&
                          static_cast<int>(expected.size()) == init_capacity;
        if (!full && (expected.empty() ||
                      static_cast<int>(random() % 4) < push_chance)) {
          queue.emplace(&counts, next_value);
          expected.push_back(next_value++);
        } else {
          queue.pop();
          expected.pop_front();
        }
        ASSERT_EQ(queue.size(), expected.size()) << "step " << i;
        if (!expected.empty()) {
          ASSERT_EQ(queue.front().GetValue(), expected.front()) << "step " << i;
          ASSERT_EQ(queue.back().GetValue(), expected.back()) << "step " << i;
        }
        if (i % 97 == 0) {
          Queue<Item> copy(queue);
          for (int value : expected) {
            ASSERT_EQ(copy.front().GetValue(), value) << "step " << i;
            copy.pop();
          }
          ASSERT_TRUE(copy.empty()) << "step " << i;
        }
      }
    }
    EXPECT_EQ(counts.init_construct + counts.copy_construct, counts.destruct);
  }
}

}  // namespace
}  // namespace gb
