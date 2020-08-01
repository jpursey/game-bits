// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_render_buffer.h"

#include "glog/logging.h"

namespace gb {

TestRenderBuffer::TestRenderBuffer(Config* config, DataVolatility volatility,
                                   int value_size, int capacity)
    : RenderBuffer(volatility, value_size, capacity), config_(config) {
  const size_t size = value_size * capacity;
  data_ = std::malloc(size);
  std::memset(data_, 0xFF, size);
}

TestRenderBuffer::~TestRenderBuffer() { std::free(data_); }

bool TestRenderBuffer::CheckBytes(int begin, int end, uint8_t value) const {
  CHECK(begin >= 0 && end >= 0 && begin <= end &&
        end <= GetCapacity() * GetValueSize());
  const uint8_t* bytes = static_cast<const uint8_t*>(data_);
  for (int i = begin; i < end; ++i) {
    if (bytes[i] != value) {
      return false;
    }
  }
  return true;
}

void TestRenderBuffer::SetBytes(int begin, int end, uint8_t value) {
  CHECK(begin >= 0 && end >= 0 && begin <= end &&
        end <= GetCapacity() * GetValueSize());
  std::memset(static_cast<uint8_t*>(data_) + begin, value, end - begin);
}

bool TestRenderBuffer::DoClear(int offset, int size) {
  if (config_->fail_clear) {
    return false;
  }
  modify_count_ += 1;

  if (editing_ || offset < 0 || size < 0 || offset + size > GetCapacity()) {
    invalid_call_count_ += 1;
    return false;
  }

  std::memset(static_cast<uint8_t*>(data_) + (offset * GetValueSize()), 0,
              size * GetValueSize());
  return true;
}

bool TestRenderBuffer::DoSet(const void* data, int size) {
  if (config_->fail_set) {
    return false;
  }
  modify_count_ += 1;

  if (size == 0) {
    return true;
  }
  if (editing_ || data == nullptr || size < 0 || size > GetCapacity()) {
    invalid_call_count_ += 1;
    return false;
  }
  std::memcpy(data_, data, size * GetValueSize());
  return true;
}

void* TestRenderBuffer::DoEditBegin() {
  if (config_->fail_edit_begin) {
    return nullptr;
  }
  if (editing_) {
    invalid_call_count_ += 1;
    return nullptr;
  }
  editing_ = true;
  return data_;
}

void TestRenderBuffer::OnEditEnd(bool modified) {
  if (modified) {
    modify_count_ += 1;
  }
  if (!editing_) {
    invalid_call_count_ += 1;
    return;
  }
  editing_ = false;
}

}  // namespace gb
