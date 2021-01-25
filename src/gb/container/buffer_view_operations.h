// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONTAINER_BUFFER_VIEW_OPERATIONS_H_
#define GB_CONTAINER_BUFFER_VIEW_OPERATIONS_H_

namespace gb {

// =============================================================================
// This functions in this header are used by the BufferView family of containers
// to perform operations on elements of the buffer.
// =============================================================================

// Constructs a value in a BufferView.
//
// This is called in the BufferView constructor to construct the type. By
// default, values are default constructed. Override either BufferViewConstruct
// or BufferViewConstructAt to customize this behavior.
template <typename Type>
void BufferViewConstruct(Type* value) {
  new (value) Type();
}
template <typename Position, typename Type>
void BufferViewConstructAt(const Position& pos, Type* value) {
  BufferViewConstruct<Type>(value);
}

// Destructs a valud in a BufferView
//
// This is called in the BufferView destructor to delete all values. By default,
// this calls the destructor. Override ufferViewDestruct to customize this
// behavior.
template <typename Type>
void BufferViewDestruct(Type* value) {
  value->~Type();
}

// Clears a value in the buffer.
//
// This is called when a BufferView Clear function is called and when the
// BufferView origin is changed to clear values that now represent a new value
// in the buffer. By default, this assigns a default constructed value onto the
// value being cleared. Override either BufferViewClear or BufferViewClearAt to
// override this behavior.
template <typename Type>
void BufferViewClear(Type& value) {
  value = Type();
}
template <typename Position, typename Type>
void BufferViewClearAt(const Position& pos, Type& value) {
  BufferViewClear<Type>(value);
}

}  // namespace gb

#endif  // GB_CONTAINER_BUFFER_VIEW_OPERATIONS_H_
