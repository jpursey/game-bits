// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONTAINER_BUFFER_VIEW_3D_H_
#define GB_CONTAINER_BUFFER_VIEW_3D_H_

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "gb/base/allocator.h"
#include "gb/container/buffer_view_operations.h"
#include "glm/glm.hpp"

namespace gb {

template <typename Type>
class BufferView3d {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Initializes the buffer view with the specified size and origin.
  //
  // The size must be a power of 2. If the origin is not specified, then the
  // buffer's origin is 0,0,0.
  explicit BufferView3d(const glm::ivec3& size,
                        const glm::ivec3& origin = {0, 0, 0});
  BufferView3d(const BufferView3d&) = delete;
  BufferView3d(BufferView3d&&) = delete;
  BufferView3d& operator=(const BufferView3d&) = delete;
  BufferView3d& operator=(BufferView3d&&) = delete;
  ~BufferView3d();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  const glm::ivec3& GetSize() const { return size_; }

  const glm::ivec3& GetOrigin() const { return origin_; }
  void SetOrigin(const glm::ivec3& origin);

  //----------------------------------------------------------------------------
  // Buffer operations
  //----------------------------------------------------------------------------

  const Type* Get(const glm::ivec3& pos) const;
  Type* Modify(const glm::ivec3& pos);
  bool Set(const glm::ivec3& pos, const Type& value);

  const Type& GetRelative(const glm::ivec3& rpos) const;
  Type& ModifyRelative(const glm::ivec3& rpos);
  void SetRelative(const glm::ivec3& rpos, const Type& value);

  void Clear(const glm::ivec3& pos, const glm::ivec3& size);
  void ClearRelative(const glm::ivec3& rpos, const glm::ivec3& size);

 private:
  static inline void NextPos(const glm::ivec3& begin_pos,
                             const glm::ivec3& end_pos, glm::ivec3& pos) {
    if (++pos.z == end_pos.z) {
      pos.z = begin_pos.z;
      if (++pos.y == end_pos.y) {
        pos.y = begin_pos.y;
        if (++pos.x == end_pos.x) {
          pos.x = begin_pos.x;
        }
      }
    }
  }
  inline glm::ivec3 FromRelative(const glm::ivec3& rpos,
                                 const glm::ivec3& delta) {
    glm::ivec3 pos = origin_ + rpos;
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
    if (pos.z - delta.z < origin_.z) {
      pos.z += size_.z;
    } else if (pos.z - delta.z >= origin_.z + size_.z) {
      pos.z -= size_.z;
    }
    return pos;
  }
  void ClearRelativeImpl(const glm::ivec3& new_origin, const glm::ivec3& rpos,
                         const glm::ivec3& size);

  const glm::ivec3 size_;
  const glm::uvec3 size_mask_;
  glm::ivec3 origin_ = {0, 0, 0};
  glm::ivec3 offset_ = {0, 0, 0};
  Type* buffer_ = nullptr;
};

template <typename Type>
BufferView3d<Type>::BufferView3d(const glm::ivec3& size,
                                 const glm::ivec3& origin)
    : size_(size),
      size_mask_(size.x - 1, size.y - 1, size.z - 1),
      origin_(origin) {
  DCHECK(size_.x > 0 && size_.y > 0 && size_.z > 0);
  DCHECK((static_cast<uint32_t>(size_.x) & size_mask_.x) == 0 &&
         (static_cast<uint32_t>(size_.y) & size_mask_.y) == 0 &&
         (static_cast<uint32_t>(size_.z) & size_mask_.z) == 0)
      << "Size dimensions must all be powers of 2";
  buffer_ = static_cast<Type*>(GetDefaultAllocator()->Alloc(
      sizeof(Type) * size_.x * size_.y * size_.z, alignof(Type)));
  glm::ivec3 pos = origin_;
  const glm::ivec3 end_pos = origin_ + size_;
  Type* const end = buffer_ + (size_.x * size_.y * size_.z);
  for (Type* it = buffer_; it != end; ++it) {
    BufferViewConstructAt(pos, it);
    NextPos(origin_, end_pos, pos);
  }
}

template <typename Type>
BufferView3d<Type>::~BufferView3d() {
  Type* const end = buffer_ + (size_.x * size_.y * size_.z);
  for (Type* it = buffer_; it != end; ++it) {
    BufferViewDestruct(it);
  }
  GetDefaultAllocator()->Free(buffer_);
}

template <typename Type>
inline const Type& BufferView3d<Type>::GetRelative(
    const glm::ivec3& rpos) const {
  const uint32_t index_x = ((rpos.x + offset_.x) & size_mask_.x);
  const uint32_t index_y = ((rpos.y + offset_.y) & size_mask_.y);
  const uint32_t index_z = ((rpos.z + offset_.z) & size_mask_.z);
  return buffer_[(index_x * size_.y + index_y) * size_.z + index_z];
}

template <typename Type>
inline Type& BufferView3d<Type>::ModifyRelative(const glm::ivec3& rpos) {
  const uint32_t index_x = ((rpos.x + offset_.x) & size_mask_.x);
  const uint32_t index_y = ((rpos.y + offset_.y) & size_mask_.y);
  const uint32_t index_z = ((rpos.z + offset_.z) & size_mask_.z);
  return buffer_[(index_x * size_.y + index_y) * size_.z + index_z];
}

template <typename Type>
inline void BufferView3d<Type>::SetRelative(const glm::ivec3& rpos,
                                            const Type& value) {
  ModifyRelative(rpos) = value;
}

template <typename Type>
inline const Type* BufferView3d<Type>::Get(const glm::ivec3& pos) const {
  glm::ivec3 rpos = pos - origin_;
  if (rpos.x < 0 || rpos.y < 0 || rpos.z < 0 || rpos.x >= size_.x ||
      rpos.y >= size_.y || rpos.z >= size_.z) {
    return nullptr;
  }
  return &GetRelative(rpos);
}

template <typename Type>
inline Type* BufferView3d<Type>::Modify(const glm::ivec3& pos) {
  glm::ivec3 rpos = pos - origin_;
  if (rpos.x < 0 || rpos.y < 0 || rpos.z < 0 || rpos.x >= size_.x ||
      rpos.y >= size_.y || rpos.z >= size_.z) {
    return nullptr;
  }
  return &ModifyRelative(rpos);
}

template <typename Type>
inline bool BufferView3d<Type>::Set(const glm::ivec3& pos, const Type& value) {
  Type* buffer_value = Modify(pos);
  if (buffer_value == nullptr) {
    return false;
  }
  *buffer_value = value;
  return true;
}

template <typename Type>
void BufferView3d<Type>::ClearRelativeImpl(const glm::ivec3& delta,
                                           const glm::ivec3& rpos,
                                           const glm::ivec3& size) {
  glm::ivec3 i;
  for (i.x = rpos.x; i.x < rpos.x + size.x; ++i.x) {
    for (i.y = rpos.y; i.y < rpos.y + size.y; ++i.y) {
      for (i.z = rpos.z; i.z < rpos.z + size.z; ++i.z) {
        BufferViewClearAt(FromRelative(i, delta), ModifyRelative(i));
      }
    }
  }
}

template <typename Type>
void BufferView3d<Type>::ClearRelative(const glm::ivec3& rpos,
                                       const glm::ivec3& size) {
  ClearRelativeImpl({0, 0, 0}, rpos, size);
}

template <typename Type>
inline void BufferView3d<Type>::Clear(const glm::ivec3& pos,
                                      const glm::ivec3& size) {
  glm::ivec3 rpos = pos - origin_;
  glm::ivec3 clear_size = size;
  if (rpos.x < 0) {
    clear_size.x += rpos.x;
    rpos.x = 0;
  }
  if (rpos.y < 0) {
    clear_size.y += rpos.y;
    rpos.y = 0;
  }
  if (rpos.z < 0) {
    clear_size.z += rpos.z;
    rpos.z = 0;
  }
  clear_size.x = std::min(clear_size.x, size_.x - rpos.x);
  clear_size.y = std::min(clear_size.y, size_.y - rpos.y);
  clear_size.z = std::min(clear_size.z, size_.z - rpos.z);
  if (clear_size.x > 0 && clear_size.y > 0 && clear_size.z > 0) {
    ClearRelative(rpos, clear_size);
  }
}

template <typename Type>
void BufferView3d<Type>::SetOrigin(const glm::ivec3& origin) {
  DCHECK(origin.x >= 0 && origin.y >= 0 && origin.z >= 0);

  glm::ivec3 delta = origin - origin_;
  if (delta.x >= size_.x || delta.x <= -size_.x || delta.y >= size_.y ||
      delta.y <= -size_.y || delta.z >= size_.z || delta.z <= -size_.z) {
    origin_ = origin;
    offset_ = {0, 0, 0};
    glm::ivec3 pos = origin_;
    const glm::ivec3 end_pos = origin_ + size_;
    Type* const end = buffer_ + (size_.x * size_.y * size_.z);
    for (Type* it = buffer_; it != end; ++it) {
      BufferViewClearAt(pos, *it);
      NextPos(origin_, end_pos, pos);
    }
    return;
  }

  int32_t start_x = 0;
  int32_t size_x = size_.x;
  if (delta.x < 0) {
    ClearRelativeImpl(delta, {size_.x + delta.x, 0, 0},
                      {-delta.x, size_.y, size_.z});
    size_x = size_.x + delta.x;
  } else if (delta.x > 0) {
    ClearRelativeImpl(delta, {0, 0, 0}, {delta.x, size_.y, size_.z});
    start_x = delta.x;
    size_x = size_.x - delta.x;
  }

  int32_t start_y = 0;
  int32_t size_y = size_.y;
  if (delta.y < 0) {
    ClearRelativeImpl(delta, {start_x, size_.y + delta.y, 0},
                      {size_x, -delta.y, size_.z});
    size_y = size_.y + delta.y;
  } else {
    ClearRelativeImpl(delta, {start_x, 0, 0}, {size_x, delta.y, size_.z});
    start_y = delta.y;
    size_y = size_.y - delta.y;
  }

  if (delta.z < 0) {
    ClearRelativeImpl(delta, {start_x, start_y, size_.z + delta.z},
                      {size_x, size_y, -delta.z});
  } else {
    ClearRelativeImpl(delta, {start_x, start_y, 0}, {size_x, size_y, delta.z});
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
  if (offset_.z < 0) {
    offset_.z += size_.z;
  } else if (offset_.z >= size_.z) {
    offset_.z -= size_.z;
  }
}

}  // namespace gb

#endif  // GB_CONTAINER_BUFFER_VIEW_3D_H_
