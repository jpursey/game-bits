// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_CIRCULAR_QUEUE_H_
#define GB_BASE_CIRCULAR_QUEUE_H_

#include <cstdlib>
#include <new>

namespace gb {

// Mostly drop-in replacement for std::queue which is implemented in terms of a
// circular queue, optionally dynamically resizable.
//
// The queue has the following guarantees:
// - Pointer stability: Pointers to any element in a queue will remain valid
//   until that element is deleted.
// - Move stability: No elements are created or deleted for move construction or
//   assignment.
// - Performance:
//   - Copy construct is O(m) where m is the number elements copied from the
//     other queue.
//   - Move assignment is O(n) where n is the number of elements in the queue
//     being moved to.
//   - Copy assignment is O(n+m) where n is the number of elements in the
//     destination queue, and m is the number of elements in the source queue.
//   - The destructor is O(n).
//   - All other methods are O(1).
//
// The queue is implemented by storing queue elements in one or more bucket(s).
// The first bucket has an initial capacity, and if the queue is full, it will
// grow by adding a bucket of the specified "grow capacity" (which may be zero,
// indicating the queue cannot grow).
//
// This class is thread-compatible.
template <typename Type>
class CircularQueue {
 public:
  //----------------------------------------------------------------------------
  // std::queue types (except container_type)
  //----------------------------------------------------------------------------

  using value_type = Type;
  using size_type = size_t;
  using reference = Type&;
  using const_reference = const Type&;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  explicit CircularQueue(size_type capacity);
  CircularQueue(size_type init_capacity, size_type grow_capacity);
  CircularQueue(const CircularQueue& other);
  CircularQueue(CircularQueue&& other);
  CircularQueue& operator=(const CircularQueue& other);
  CircularQueue& operator=(CircularQueue&& other);
  ~CircularQueue();

  //----------------------------------------------------------------------------
  // std::queue compliant methods
  //----------------------------------------------------------------------------

  bool empty() const;
  size_type size() const;

  reference front();
  const_reference front() const;
  reference back();
  const_reference back() const;

  void push(const value_type& value);
  void push(value_type&& value);
  template <class... Args>
  decltype(auto) emplace(Args&&... args);
  void pop();

  void swap(CircularQueue& other) noexcept;

  //----------------------------------------------------------------------------
  // Extended methods
  //----------------------------------------------------------------------------

  size_type capacity() const;
  size_type grow_capacity() const;

 private:
  static inline constexpr size_type kInvalidIndex =
      std::numeric_limits<size_type>::max();

  struct Bucket {
    Bucket() = default;

    // True if this bucket can be pushed to at the front. This is set to true
    // when back_ points to the first element, and is cleared when front_
    // consumes the first element.
    bool can_push = false;

    // How many elements this bucket can hold.
    size_type capacity = 0;

    // Next bucket in the list (may be the same bucket)
    Bucket* next = nullptr;
    Bucket* prev = nullptr;

    // These are only set if the queue was full and a new element was pushed. If
    // this occurs in the middle of a bucket, then these are set to where the
    // last push was made to this bucket, and to where the next pop should be
    // from when this bucket is emptied.
    size_type push_end = kInvalidIndex;
    Bucket* pop_next = nullptr;

    Type* Data(size_type index = 0) {
      return reinterpret_cast<Type*>(this + 1) + index;
    }
  };

  struct Position {
    Position() = default;
    Bucket* bucket = nullptr;
    size_type index = 0;

    bool operator==(const Position& other) const {
      return bucket == other.bucket && index == other.index;
    }
    bool operator!=(const Position& other) const { return !(*this == other); }
  };

  Bucket* NewBucket(size_type capacity);
  Bucket* AddBucket(Bucket* prev_bucket);
  void Advance(Position* it) const;
  void Init(size_type init_capacity);
  void Clear();
  void Copy(const CircularQueue& other);
  Type* PushAlloc();

  // Size of buckets to add when the queue is full. This is zero if the queue is
  // not resizable.
  const size_type grow_capacity_;

  size_type capacity_ = 0;     // Total capacity of all buckets.
  size_type size_ = 0;         // Current number of elements in the queue.
  Bucket* buckets_ = nullptr;  // Pointer to first bucket.
  Position front_;             // Front of the queue.
  Position back_;              // Back of the queue.
};

template <typename Type>
typename CircularQueue<Type>::Bucket* CircularQueue<Type>::NewBucket(
    size_type capacity) {
  static_assert(alignof(Bucket) >= alignof(Type));
  if (capacity == 0) {
    return nullptr;
  }
  Bucket* bucket =
      new (std::malloc(sizeof(Bucket) + sizeof(Type) * capacity)) Bucket;
  bucket->capacity = capacity;
  bucket->next = bucket->prev = bucket;
  capacity_ += capacity;
  return bucket;
}

template <typename Type>
typename CircularQueue<Type>::Bucket* CircularQueue<Type>::AddBucket(
    Bucket* prev_bucket) {
  Bucket* bucket = NewBucket(grow_capacity_);
  if (bucket == nullptr) {
    return nullptr;
  }
  bucket->next = prev_bucket->next;
  bucket->prev = prev_bucket;
  bucket->next->prev = bucket->prev->next = bucket;
  return bucket;
}

template <typename Type>
void CircularQueue<Type>::Advance(Position* it) const {
  if (it->index == it->bucket->push_end) {
    it->index = 0;
    it->bucket = it->bucket->next;
  } else if (++it->index == it->bucket->capacity) {
    it->index = 0;
    it->bucket = (it->bucket->pop_next != nullptr ? it->bucket->pop_next
                                                  : it->bucket->next);
  }
}

template <typename Type>
void CircularQueue<Type>::Init(size_type init_capacity) {
  front_.bucket = back_.bucket = buckets_ =
      NewBucket(init_capacity > 0 ? init_capacity + 1 : init_capacity);
  capacity_ = init_capacity;
}

template <typename Type>
void CircularQueue<Type>::Clear() {
  while (front_ != back_) {
    front_.bucket->Data(front_.index)->~Type();
    Advance(&front_);
  }
  if (buckets_ != nullptr) {
    Bucket* bucket = buckets_;
    do {
      Bucket* next = bucket->next;
      std::free(bucket);
      bucket = next;
    } while (bucket != buckets_);
  }
  capacity_ = 0;
  size_ = 0;
  buckets_ = nullptr;
  front_ = {};
  back_ = {};
}

template <typename Type>
void CircularQueue<Type>::Copy(const CircularQueue& other) {
  size_type init_capacity = other.size_;
  if (other.buckets_ != nullptr && other.buckets_->capacity > init_capacity) {
    init_capacity = other.buckets_->capacity;
  }
  Init(init_capacity);

  Position it = other.front_;
  size_type i = 0;
  while (it != other.back_) {
    new (buckets_->Data(i++)) Type(*it.bucket->Data(it.index));
    other.Advance(&it);
  }
  back_.index = size_ = other.size_;
}

template <typename Type>
Type* CircularQueue<Type>::PushAlloc() {
  if (back_.bucket == nullptr) {
    Init(grow_capacity_);
  }
  Type* new_data = back_.bucket->Data(back_.index);

  Position new_back = back_;
  if (++new_back.index == back_.bucket->capacity) {
    new_back.index = 0;
    new_back.bucket = back_.bucket->next;
    if (!new_back.bucket->can_push ||
        new_back.bucket->push_end != kInvalidIndex) {
      new_back.bucket = AddBucket(back_.bucket);
      if (new_back.bucket == nullptr) {
        return nullptr;
      }
    }
  } else if (new_back == front_) {
    Bucket* pop_next = back_.bucket->next;
    new_back.bucket = AddBucket(back_.bucket);
    if (new_back.bucket == nullptr) {
      return nullptr;
    }
    new_back.index = 0;
    back_.bucket->pop_next = pop_next;
    back_.bucket->push_end = back_.index;
  }

  back_ = new_back;
  if (back_.index == 0) {
    back_.bucket->can_push = false;
  }
  ++size_;
  return new_data;
}

template <typename Type>
CircularQueue<Type>::CircularQueue(size_type init_capacity,
                                   size_type grow_capacity)
    : grow_capacity_(grow_capacity) {
  Init(init_capacity);
}

template <typename Type>
CircularQueue<Type>::CircularQueue(size_type capacity)
    : CircularQueue(capacity, capacity) {}

template <typename Type>
CircularQueue<Type>::CircularQueue(const CircularQueue& other)
    : grow_capacity_(other.grow_capacity_) {
  Copy(other);
}

template <typename Type>
CircularQueue<Type>::CircularQueue(CircularQueue&& other)
    : grow_capacity_(other.grow_capacity_),
      capacity_(std::exchange(other.capacity_, 0)),
      size_(std::exchange(other.size_, 0)),
      buckets_(std::exchange(other.buckets_, nullptr)),
      front_(std::exchange(other.front_, Position{})),
      back_(std::exchange(other.back_, Position{})) {}

template <typename Type>
CircularQueue<Type>& CircularQueue<Type>::operator=(
    const CircularQueue& other) {
  if (&other == this) {
    return *this;
  }
  Clear();
  Copy(other);
  return *this;
}

template <typename Type>
CircularQueue<Type>& CircularQueue<Type>::operator=(CircularQueue&& other) {
  if (&other == this) {
    return *this;
  }
  Clear();
  capacity_ = std::exchange(other.capacity_, 0);
  size_ = std::exchange(other.size_, 0);
  buckets_ = std::exchange(other.buckets_, nullptr);
  front_ = std::exchange(other.front_, Position{});
  back_ = std::exchange(other.back_, Position{});
  return *this;
}

template <typename Type>
CircularQueue<Type>::~CircularQueue() {
  Clear();
}

template <typename Type>
bool CircularQueue<Type>::empty() const {
  return size_ == 0;
}

template <typename Type>
typename CircularQueue<Type>::size_type CircularQueue<Type>::size() const {
  return size_;
}

template <typename Type>
typename CircularQueue<Type>::size_type CircularQueue<Type>::capacity() const {
  return capacity_;
}

template <typename Type>
typename CircularQueue<Type>::size_type CircularQueue<Type>::grow_capacity()
    const {
  return grow_capacity_;
}

template <typename Type>
typename CircularQueue<Type>::reference CircularQueue<Type>::front() {
  return *front_.bucket->Data(front_.index);
}

template <typename Type>
typename CircularQueue<Type>::const_reference CircularQueue<Type>::front()
    const {
  return *front_.bucket->Data(front_.index);
}

template <typename Type>
typename CircularQueue<Type>::reference CircularQueue<Type>::back() {
  if (back_.index > 0) {
    return *back_.bucket->Data(back_.index - 1);
  }
  Bucket* bucket = back_.bucket->prev;
  if (bucket->push_end != kInvalidIndex) {
    return *bucket->Data(bucket->push_end);
  }
  return *bucket->Data(bucket->capacity - 1);
}

template <typename Type>
typename CircularQueue<Type>::const_reference CircularQueue<Type>::back()
    const {
  return const_cast<CircularQueue<Type>*>(this)->back();
}

template <typename Type>
void CircularQueue<Type>::push(const value_type& value) {
  new (PushAlloc()) Type(value);
}

template <typename Type>
void CircularQueue<Type>::push(value_type&& value) {
  new (PushAlloc()) Type(std::move(value));
}

template <typename Type>
template <class... Args>
decltype(auto) CircularQueue<Type>::emplace(Args&&... args) {
  Type* value = PushAlloc();
  new (value) Type(std::forward<Args>(args)...);
  return *value;
}

template <typename Type>
void CircularQueue<Type>::pop() {
  front().~Type();
  Position new_front = front_;
  Advance(&new_front);
  if (new_front.index == 0) {
    if (front_.index == front_.bucket->push_end) {
      front_.bucket->push_end = kInvalidIndex;
    } else if (front_.index == front_.bucket->capacity - 1 &&
               front_.bucket->pop_next != nullptr) {
      front_.bucket->pop_next = nullptr;
    }
  }
  if (front_.index == 0) {
    front_.bucket->can_push = true;
  }
  front_ = new_front;
  --size_;
}

template <typename Type>
void CircularQueue<Type>::swap(CircularQueue& other) noexcept {
  std::swap(other.capacity_, capacity_);
  std::swap(other.size_, size_);
  std::swap(other.buckets_, buckets_);
  std::swap(other.front_, front_);
  std::swap(other.back_, back_);
}

}  // namespace gb

namespace std {

template <typename Type>
void swap(gb::CircularQueue<Type>& a, gb::CircularQueue<Type>& b) noexcept {
  a.swap(b);
}

}  // namespace std

#endif  // GB_BASE_CIRCULAR_QUEUE_H_
