// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONTAINER_BUFFER_VIEW_H_
#define GB_CONTAINER_BUFFER_VIEW_H_

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "gb/base/allocator.h"
#include "gb/container/buffer_view_operations.h"

namespace gb {

template <typename Type>
class BufferView {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Initializes the buffer view with the specified size and origin.
  //
  // The size must be a power of 2. If the origin is not specified, then the
  // buffer's origin is 0.
  explicit BufferView(int32_t size, int32_t origin = 0);
  BufferView(const BufferView&) = delete;
  BufferView(BufferView&&) = delete;
  BufferView& operator=(const BufferView&) = delete;
  BufferView& operator=(BufferView&&) = delete;
  ~BufferView();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  int32_t GetSize() const { return size_; }

  int32_t GetOrigin() const { return origin_; }
  void SetOrigin(int32_t origin);

  //----------------------------------------------------------------------------
  // Buffer operations
  //----------------------------------------------------------------------------

  const Type* Get(int32_t pos) const;
  Type* Modify(int32_t pos);
  bool Set(int32_t pos, const Type& value);

  const Type& GetRelative(int32_t rpos) const;
  Type& ModifyRelative(int32_t rpos);
  void SetRelative(int32_t rpos, const Type& value);

  void Clear(int32_t pos, int32_t size);
  void ClearRelative(int32_t rpos, int32_t size);

 private:
  static inline void NextPos(int32_t begin_pos, int32_t end_pos, int32_t& pos) {
    if (++pos == end_pos) {
      pos = begin_pos;
    }
  }
  void ClearRelativeImpl(int32_t new_origin, int32_t rpos, int32_t size);

  const int32_t size_;
  const uint32_t size_mask_;
  int32_t origin_ = 0;
  int32_t offset_ = 0;
  Type* buffer_ = nullptr;
};

template <typename Type>
BufferView<Type>::BufferView(int32_t size, int32_t origin)
    : size_(size), size_mask_(size - 1), origin_(origin) {
  DCHECK(size_ > 0);
  DCHECK((static_cast<uint32_t>(size_) & size_mask_) == 0)
      << "Size must be power of 2";
  buffer_ = static_cast<Type*>(
      GetDefaultAllocator()->Alloc(sizeof(Type) * size_, alignof(Type)));
  int32_t pos = origin_;
  const int32_t end_pos = origin_ + size_;
  Type* const end = buffer_ + size_;
  for (Type* it = buffer_; it != end; ++it) {
    BufferViewConstructAt(pos, it);
    NextPos(origin_, end_pos, pos);
  }
}

template <typename Type>
BufferView<Type>::~BufferView() {
  Type* const end = buffer_ + size_;
  for (Type* it = buffer_; it != end; ++it) {
    BufferViewDestruct(it);
  }
  GetDefaultAllocator()->Free(buffer_);
}

template <typename Type>
inline const Type& BufferView<Type>::GetRelative(int32_t rpos) const {
  return buffer_[(rpos + offset_) & size_mask_];
}

template <typename Type>
inline Type& BufferView<Type>::ModifyRelative(int32_t rpos) {
  return buffer_[(rpos + offset_) & size_mask_];
}

template <typename Type>
inline void BufferView<Type>::SetRelative(int32_t rpos, const Type& value) {
  ModifyRelative(rpos) = value;
}

template <typename Type>
inline const Type* BufferView<Type>::Get(int32_t pos) const {
  int32_t rpos = pos - origin_;
  if (rpos < 0 || rpos >= size_) {
    return nullptr;
  }
  return &GetRelative(rpos);
}

template <typename Type>
inline Type* BufferView<Type>::Modify(int32_t pos) {
  int32_t rpos = pos - origin_;
  if (rpos < 0 || rpos >= size_) {
    return nullptr;
  }
  return &ModifyRelative(rpos);
}

template <typename Type>
inline bool BufferView<Type>::Set(int32_t pos, const Type& value) {
  Type* buffer_value = Modify(pos);
  if (buffer_value == nullptr) {
    return false;
  }
  *buffer_value = value;
  return true;
}

template <typename Type>
void BufferView<Type>::ClearRelativeImpl(int32_t new_origin, int32_t rpos,
                                         int32_t size) {
  for (int32_t i = rpos; i < rpos + size; ++i) {
    BufferViewClearAt(new_origin + i, ModifyRelative(i));
  }
}

template <typename Type>
void BufferView<Type>::ClearRelative(int32_t rpos, int32_t size) {
  ClearRelativeImpl(origin_, rpos, size);
}

template <typename Type>
inline void BufferView<Type>::Clear(int32_t pos, int32_t size) {
  int32_t rpos = pos - origin_;
  int32_t clear_size = size;
  if (rpos < 0) {
    clear_size += rpos;
    rpos = 0;
  }
  clear_size = std::min(clear_size, size_ - rpos);
  if (clear_size > 0) {
    ClearRelative(rpos, clear_size);
  }
}

template <typename Type>
void BufferView<Type>::SetOrigin(int32_t origin) {
  DCHECK(origin >= 0);

  int32_t delta = origin - origin_;
  if (delta >= size_ || delta <= -size_) {
    origin_ = origin;
    offset_ = 0;
    int32_t pos = origin_;
    const int32_t end_pos = origin_ + size_;
    Type* const end = buffer_ + size_;
    for (Type* it = buffer_; it != end; ++it) {
      BufferViewClearAt(pos, *it);
      NextPos(origin_, end_pos, pos);
    }
    return;
  }

  if (delta < 0) {
    ClearRelativeImpl(origin_ - size_, size_ + delta, -delta);
  } else {
    ClearRelativeImpl(origin_ + size_, 0, delta);
  }

  origin_ = origin;
  offset_ += delta;
  if (offset_ < 0) {
    offset_ += size_;
  } else if (offset_ >= size_) {
    offset_ -= size_;
  }
}

}  // namespace gb

#endif  // GB_CONTAINER_BUFFER_VIEW_H_
