// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONTAINER_BUFFER_VIEW_TEST_TYPES_H_
#define GB_CONTAINER_BUFFER_VIEW_TEST_TYPES_H_

#include <ostream>

#include "gb/container/buffer_view_operations.h"
#include "glm/glm.hpp"

inline std::ostream& operator<<(std::ostream& out, const glm::ivec2& pos) {
  out << "(" << pos.x << "," << pos.y << ")";
  return out;
}

inline std::ostream& operator<<(std::ostream& out, const glm::ivec3& pos) {
  out << "(" << pos.x << "," << pos.y << "," << pos.z << ")";
  return out;
}

namespace gb {

enum class OpType {
  kInvalid,
  kConstruct,
  kDestruct,
  kClear,
};

template <typename ValueType>
struct Operation {
  Operation() = default;
  Operation(OpType in_type, ValueType in_value, ValueType in_old_value = {})
      : type(in_type), value(in_value), old_value(in_old_value) {}
  OpType type = OpType::kInvalid;
  ValueType value = {};
  ValueType old_value = {};
};

template <typename ValueType>
inline bool operator==(const Operation<ValueType>& a,
                       const Operation<ValueType>& b) {
  return a.type == b.type && a.value == b.value && a.old_value == b.old_value;
}

template <typename ValueType>
inline void PrintTo(const Operation<ValueType>& op, std::ostream* out) {
  switch (op.type) {
    case OpType::kConstruct:
      *out << "Construct:{" << op.value << "}";
      break;
    case OpType::kDestruct:
      *out << "Destruct:{" << op.value << "}";
      break;
    case OpType::kClear:
      *out << "Clear:{old=" << op.old_value << ",new=" << op.value << "}";
      break;
    default:
      *out << "Invalid";
      break;
  }
}

using IntOp = Operation<int32_t>;
using Vec2Op = Operation<glm::ivec2>;
using Vec3Op = Operation<glm::ivec3>;

template <typename ValueType>
std::vector<Operation<ValueType>>& GetOperations() {
  static std::vector<Operation<ValueType>> operations;
  return operations;
}

template <typename ValueType>
void ResetOperations() {
  GetOperations<ValueType>().clear();
}

template <typename ValueType = int32_t>
void AddOperation(OpType type, ValueType value, ValueType old_value = {}) {
  GetOperations<ValueType>().emplace_back(type, value, old_value);
}

struct DefaultItem {
  DefaultItem() : value(-1) { AddOperation(OpType::kConstruct, value); }
  DefaultItem(const DefaultItem&) = delete;
  DefaultItem(DefaultItem&&) = delete;
  DefaultItem& operator=(const DefaultItem&) = delete;
  DefaultItem& operator=(DefaultItem&& other) {
    AddOperation(OpType::kClear, other.value, value);
    value = other.value;
    return *this;
  }
  ~DefaultItem() { AddOperation(OpType::kDestruct, value); }
  int value = 0;
};

struct Item {
  explicit Item(int in_value) : value(in_value) {
    AddOperation(OpType::kConstruct, value);
  }
  Item(const Item&) = delete;
  Item(Item&&) = delete;
  Item& operator=(const Item& other) = delete;
  Item& operator=(Item&& other) = delete;
  ~Item() { AddOperation(OpType::kDestruct, value); }

  void Clear() {
    AddOperation(OpType::kClear, -1, value);
    value = -1;
  }
  int value = 0;
};

template <typename PosType>
struct PosItem {
  explicit PosItem(const PosType& in_pos) : pos(in_pos) {
    AddOperation(OpType::kConstruct, pos);
  }
  PosItem(const PosItem&) = delete;
  PosItem(PosItem&&) = delete;
  PosItem& operator=(const PosItem& other) = delete;
  PosItem& operator=(PosItem&& other) = delete;
  ~PosItem() { AddOperation(OpType::kDestruct, pos); }

  void Clear(const PosType& in_pos) {
    AddOperation(OpType::kClear, in_pos, pos);
    pos = in_pos;
  }

  PosType pos;
};

template <>
inline void BufferViewConstruct(Item* item) {
  new (item) Item(-1);
}

template <>
inline void BufferViewDestruct(Item* item) {
  item->value = -2;
  item->~Item();
}

template <>
inline void BufferViewClear(Item& item) {
  item.Clear();
}

template <typename PosType>
inline void BufferViewConstructAt(const PosType& pos, PosItem<PosType>* item) {
  new (item) PosItem<PosType>(pos);
}

template <typename PosType>
inline void BufferViewDestruct(PosItem<PosType>* item) {
  item->~PosItem<PosType>();
}

template <typename PosType>
inline void BufferViewClearAt(const PosType& pos, PosItem<PosType>& item) {
  item.Clear(pos);
}

}  // namespace gb

#endif  // GB_CONTAINER_BUFFER_VIEW_TEST_TYPES_H_
