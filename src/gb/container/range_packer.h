// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONTAINER_RANGE_PACKER_H_
#define GB_CONTAINER_RANGE_PACKER_H_

#include <optional>
#include <utility>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"

namespace gb {

// Packs ranges of offsets into a capacity, such as places in a GPU buffer,
// keeping track of the gaps between them. It holds offsets only, never memory.
//
// Each range is placed in the smallest gap it fits (the one at the lowest
// offset, of those of equal size), and a removed range merges with the gaps on
// either side. Placing and removing take O(log n) time in the number of gaps.
//
// This class is thread-compatible.
class RangePacker final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a packer with every offset from zero up to `capacity` in one gap.
  // `capacity` must be positive.
  explicit RangePacker(int capacity);
  RangePacker(const RangePacker&) = delete;
  RangePacker& operator=(const RangePacker&) = delete;
  ~RangePacker() = default;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  int GetCapacity() const { return capacity_; }

  // Returns the number of offsets in placed ranges.
  int GetUsed() const { return used_; }

  // Returns the number of gaps, which are never next to each other.
  int GetGapCount() const { return static_cast<int>(gaps_.size()); }

  // Returns the size of the largest gap, which is the largest range that can
  // be placed, or zero if there are no gaps.
  int GetLargestGap() const;

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Places a range of `size` offsets, returning its first offset, or nullopt
  // if no gap is big enough. `size` must be positive.
  std::optional<int> Place(int size);

  // Removes the `size` offsets from `offset`, which must be a range Place()
  // returned, leaving a gap.
  void Remove(int offset, int size);

  // Increases the capacity to `capacity`, which must be more than the current
  // capacity, adding the new offsets as a gap.
  void Grow(int capacity);

 private:
  // Adds the range as a gap, merging it with the gaps on either side.
  void AddGap(int offset, int size);

  // Inserts the gap into both maps, as it is, with no merging.
  void InsertGap(int offset, int size);

  // Removes the gap at `it` from both maps, returning the gap after it. Like
  // any erase from a btree, this invalidates other iterators.
  absl::btree_map<int, int>::iterator RemoveGap(
      absl::btree_map<int, int>::iterator it);

  int capacity_;
  int used_ = 0;

  // Gaps as offset to size, and as (size, offset) to find the best fit.
  absl::btree_map<int, int> gaps_;
  absl::btree_set<std::pair<int, int>> gaps_by_size_;
};

}  // namespace gb

#endif  // GB_CONTAINER_RANGE_PACKER_H_
