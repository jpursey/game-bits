// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_RENDER_BUFFER_H_
#define GB_RENDER_TEST_RENDER_BUFFER_H_

#include "gb/render/render_buffer.h"

namespace gb {

class TestRenderBuffer final : public RenderBuffer {
 public:
  struct Config {
    bool fail_clear = false;
    bool fail_set = false;
    bool fail_edit_begin = false;
  };

  TestRenderBuffer(Config* config, DataVolatility volatility, int value_size,
                   int capacity);
  ~TestRenderBuffer() override;

  bool CheckBytes(int begin, int end, uint8_t value) const;
  void SetBytes(int begin, int end, uint8_t value);
  void* GetData() const { return data_; }
  int GetModifyCount() const { return modify_count_; }
  int GetInvalidCallCount() const { return invalid_call_count_; }

 protected:
  bool DoClear(int offset, int size) override;
  bool DoSet(const void* data, int size) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;

 private:
  Config* const config_;
  void* data_ = nullptr;
  bool editing_ = false;
  int modify_count_ = 0;
  int invalid_call_count_ = 0;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_RENDER_BUFFER_H_
