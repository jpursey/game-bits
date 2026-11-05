// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/container/range_packer.h"

#include <iterator>

#include "absl/log/check.h"

namespace gb {

RangePacker::RangePacker(int capacity) : capacity_(capacity) {
  DCHECK_GT(capacity, 0);
  InsertGap(0, capacity);
}

int RangePacker::GetLargestGap() const {
  if (gaps_by_size_.empty()) {
    return 0;
  }
  return gaps_by_size_.rbegin()->first;
}

std::optional<int> RangePacker::Place(int size) {
  DCHECK_GT(size, 0);
  auto best = gaps_by_size_.lower_bound({size, 0});
  if (best == gaps_by_size_.end()) {
    return std::nullopt;
  }
  const auto [gap_size, offset] = *best;
  gaps_by_size_.erase(best);
  gaps_.erase(offset);
  if (gap_size > size) {
    InsertGap(offset + size, gap_size - size);
  }
  used_ += size;
  return offset;
}

void RangePacker::Remove(int offset, int size) {
  DCHECK(size > 0 && offset >= 0 && offset + size <= capacity_);
  DCHECK_LE(size, used_);
  used_ -= size;
  AddGap(offset, size);
}

void RangePacker::Grow(int capacity) {
  DCHECK_GT(capacity, capacity_);
  AddGap(capacity_, capacity - capacity_);
  capacity_ = capacity;
}

void RangePacker::AddGap(int offset, int size) {
  int end = offset + size;

  // Merge with the gap after it, if it starts where this ends.
  auto next = gaps_.lower_bound(offset);
  DCHECK(next == gaps_.end() || next->first >= end) << "Range overlaps a gap";
  if (next != gaps_.end() && next->first == end) {
    end += next->second;
    next = RemoveGap(next);
  }

  // Merge with the gap before it, if it ends where this starts.
  if (next != gaps_.begin()) {
    auto prev = std::prev(next);
    DCHECK_LE(prev->first + prev->second, offset) << "Range overlaps a gap";
    if (prev->first + prev->second == offset) {
      offset = prev->first;
      RemoveGap(prev);
    }
  }

  InsertGap(offset, end - offset);
}

void RangePacker::InsertGap(int offset, int size) {
  gaps_.emplace(offset, size);
  gaps_by_size_.emplace(size, offset);
}

absl::btree_map<int, int>::iterator RangePacker::RemoveGap(
    absl::btree_map<int, int>::iterator it) {
  gaps_by_size_.erase({it->second, it->first});
  return gaps_.erase(it);
}

}  // namespace gb
