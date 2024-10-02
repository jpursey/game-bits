// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONTAINER_BUFFER_VIEW_2D_H_
#define GB_CONTAINER_BUFFER_VIEW_2D_H_

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "gb/base/allocator.h"
#include "gb/container/buffer_view_operations.h"
#include "glm/glm.hpp"

namespace gb {

template <typename Type>
class BufferView2d {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Initializes the buffer view with the specified size and origin.
  //
  // The size must be a power of 2. If the origin is not specified, then the
  // buffer's origin is 0,0.
  explicit BufferView2d(const glm::ivec2& size,
                        const glm::ivec2& origin = {0, 0});
  BufferView2d(const BufferView2d&) = delete;
  BufferView2d(BufferView2d&&) = delete;
  BufferView2d& operator=(const BufferView2d&) = delete;
  BufferView2d& operator=(BufferView2d&&) = delete;
  ~BufferView2d();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  const glm::ivec2& GetSize() const { return size_; }

  const glm::ivec2& GetOrigin() const { return origin_; }
  void SetOrigin(const glm::ivec2& origin);

  //----------------------------------------------------------------------------
  // Buffer operations
  //----------------------------------------------------------------------------

  const Type* Get(const glm::ivec2& pos) const;
  Type* Modify(const glm::ivec2& pos);
  bool Set(const glm::ivec2& pos, const Type& value);

  const Type& GetRelative(const glm::ivec2& rpos) const;
  Type& ModifyRelative(const glm::ivec2& rpos);
  void SetRelative(const glm::ivec2& rpos, const Type& value);

  void Clear(const glm::ivec2& pos, const glm::ivec2& size);
  void ClearRelative(const glm::ivec2& rpos, const glm::ivec2& size);

 private:
  static inline void NextPos(const glm::ivec2& begin_pos,
                             const glm::ivec2& end_pos, glm::ivec2& pos) {
    if (++pos.y == end_pos.y) {
      pos.y = begin_pos.y;
      if (++pos.x == end_pos.x) {
        pos.x = begin_pos.x;
      }
    }
  }
  inline glm::ivec2 FromRelative(const glm::ivec2& rpos,
                                 const glm::ivec2& delta) {
    glm::ivec2 pos = origin_ + rpos;
    if (pos.x - delta.x < origin_.x) {
      pos.x += size_.x;
    } else if (pos.x - delta.x >= origin_.x + size_.x) {
      pos.x -= size_.x;
    }
    if (pos.y - delta.y < origin_.y) {
      pos.y += size_.y;
    } else if (pos.y - delta.y >= origin_.y + size_.y) {
      pos.y -= size_.y;
    }
    return pos;
  }
  void ClearRelativeImpl(const glm::ivec2& new_origin, const glm::ivec2& rpos,
                         const glm::ivec2& size);

  const glm::ivec2 size_;
  const glm::uvec2 size_mask_;
  glm::ivec2 origin_ = {0, 0};
  glm::ivec2 offset_ = {0, 0};
  Type* buffer_ = nullptr;
};

template <typename Type>
BufferView2d<Type>::BufferView2d(const glm::ivec2& size,
                                 const glm::ivec2& origin)
    : size_(size), size_mask_(size.x - 1, size.y - 1), origin_(origin) {
  DCHECK(size_.x > 0 && size_.y > 0);
  DCHECK((static_cast<uint32_t>(size_.x) & size_mask_.x) == 0 &&
         (static_cast<uint32_t>(size_.y) & size_mask_.y) == 0)
      << "Size dimensions must all be powers of 2";
  buffer_ = static_cast<Type*>(GetDefaultAllocator()->Alloc(
      sizeof(Type) * size_.x * size_.y, alignof(Type)));
  glm::ivec2 pos = origin_;
  const glm::ivec2 end_pos = origin_ + size_;
  Type* const end = buffer_ + (size_.x * size_.y);
  for (Type* it = buffer_; it != end; ++it) {
    BufferViewConstructAt(pos, it);
    NextPos(origin_, end_pos, pos);
  }
}

template <typename Type>
BufferView2d<Type>::~BufferView2d() {
  Type* const end = buffer_ + (size_.x * size_.y);
  for (Type* it = buffer_; it != end; ++it) {
    BufferViewDestruct(it);
  }
  GetDefaultAllocator()->Free(buffer_);
}

template <typename Type>
inline const Type& BufferView2d<Type>::GetRelative(
    const glm::ivec2& rpos) const {
  const uint32_t index_x = ((rpos.x + offset_.x) & size_mask_.x);
  const uint32_t index_y = ((rpos.y + offset_.y) & size_mask_.y);
  return buffer_[index_x * size_.y + index_y];
}

template <typename Type>
inline Type& BufferView2d<Type>::ModifyRelative(const glm::ivec2& rpos) {
  const uint32_t index_x = ((rpos.x + offset_.x) & size_mask_.x);
  const uint32_t index_y = ((rpos.y + offset_.y) & size_mask_.y);
  return buffer_[index_x * size_.y + index_y];
}

template <typename Type>
inline void BufferView2d<Type>::SetRelative(const glm::ivec2& rpos,
                                            const Type& value) {
  ModifyRelative(rpos) = value;
}

template <typename Type>
inline const Type* BufferView2d<Type>::Get(const glm::ivec2& pos) const {
  glm::ivec2 rpos = pos - origin_;
  if (rpos.x < 0 || rpos.y < 0 || rpos.x >= size_.x || rpos.y >= size_.y) {
    return nullptr;
  }
  return &GetRelative(rpos);
}

template <typename Type>
inline Type* BufferView2d<Type>::Modify(const glm::ivec2& pos) {
  glm::ivec2 rpos = pos - origin_;
  if (rpos.x < 0 || rpos.y < 0 || rpos.x >= size_.x || rpos.y >= size_.y) {
    return nullptr;
  }
  return &ModifyRelative(rpos);
}

template <typename Type>
inline bool BufferView2d<Type>::Set(const glm::ivec2& pos, const Type& value) {
  Type* buffer_value = Modify(pos);
  if (buffer_value == nullptr) {
    return false;
  }
  *buffer_value = value;
  return true;
}

template <typename Type>
void BufferView2d<Type>::ClearRelativeImpl(const glm::ivec2& delta,
                                           const glm::ivec2& rpos,
                                           const glm::ivec2& size) {
  glm::ivec2 i;
  for (i.x = rpos.x; i.x < rpos.x + size.x; ++i.x) {
    for (i.y = rpos.y; i.y < rpos.y + size.y; ++i.y) {
      BufferViewClearAt(FromRelative(i, delta), ModifyRelative(i));
    }
  }
}

template <typename Type>
void BufferView2d<Type>::ClearRelative(const glm::ivec2& rpos,
                                       const glm::ivec2& size) {
  ClearRelativeImpl({0, 0}, rpos, size);
}

template <typename Type>
inline void BufferView2d<Type>::Clear(const glm::ivec2& pos,
                                      const glm::ivec2& size) {
  glm::ivec2 rpos = pos - origin_;
  glm::ivec2 clear_size = size;
  if (rpos.x < 0) {
    clear_size.x += rpos.x;
    rpos.x = 0;
  }
  if (rpos.y < 0) {
    clear_size.y += rpos.y;
    rpos.y = 0;
  }
  clear_size.x = std::min(clear_size.x, size_.x - rpos.x);
  clear_size.y = std::min(clear_size.y, size_.y - rpos.y);
  if (clear_size.x > 0 && clear_size.y > 0) {
    ClearRelative(rpos, clear_size);
  }
}

template <typename Type>
void BufferView2d<Type>::SetOrigin(const glm::ivec2& origin) {
  DCHECK(origin.x >= 0 && origin.y >= 0);

  glm::ivec2 delta = origin - origin_;
  if (delta.x >= size_.x || delta.x <= -size_.x || delta.y >= size_.y ||
      delta.y <= -size_.y) {
    origin_ = origin;
    offset_ = {0, 0};
    glm::ivec2 pos = origin_;
    const glm::ivec2 end_pos = origin_ + size_;
    Type* const end = buffer_ + (size_.x * size_.y);
    for (Type* it = buffer_; it != end; ++it) {
      BufferViewClearAt(pos, *it);
      NextPos(origin_, end_pos, pos);
    }
    return;
  }

  int32_t start_x = 0;
  int32_t size_x = size_.x;
  if (delta.x < 0) {
    ClearRelativeImpl(delta, {size_.x + delta.x, 0}, {-delta.x, size_.y});
    size_x = size_.x + delta.x;
  } else if (delta.x > 0) {
    ClearRelativeImpl(delta, {0, 0}, {delta.x, size_.y});
    start_x = delta.x;
    size_x = size_.x - delta.x;
  }

  if (delta.y < 0) {
    ClearRelativeImpl(delta, {start_x, size_.y + delta.y}, {size_x, -delta.y});
  } else {
    ClearRelativeImpl(delta, {start_x, 0}, {size_x, delta.y});
  }

  origin_ = origin;
  offset_ += delta;
  if (offset_.x < 0) {
    offset_.x += size_.x;
  } else if (offset_.x >= size_.x) {
    offset_.x -= size_.x;
  }
  if (offset_.y < 0) {
    offset_.y += size_.y;
  } else if (offset_.y >= size_.y) {
    offset_.y -= size_.y;
  }
}

}  // namespace gb

#endif  // GB_CONTAINER_BUFFER_VIEW_2D_H_
