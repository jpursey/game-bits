// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_buffer_view.h"

#include "gb/render/render_buffer.h"
#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class RenderBufferViewTest : public RenderTest {
 protected:
  static constexpr int kValueSize = 4;
  static constexpr int kCapacity = 16;
  static constexpr int kByteSize = kValueSize * kCapacity;
  static constexpr uint32_t kData[kCapacity] = {
      0x00000000, 0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x55555555,
      0x66666666, 0x77777777, 0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB,
      0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF,
  };
  static_assert(sizeof(kData) == kByteSize);

  RenderBufferViewTest()
      : test_render_buffer_(&config, DataVolatility::kPerFrame, kValueSize,
                            kCapacity),
        render_buffer(test_render_buffer_) {}

  TestRenderBuffer::Config config;
  TestRenderBuffer test_render_buffer_;
  RenderBuffer& render_buffer;
};

TEST_F(RenderBufferViewTest, Properties) {
  auto view = render_buffer.Edit();
  ASSERT_NE(view, nullptr);

  EXPECT_EQ(view->GetCapacity(), kCapacity);
  EXPECT_EQ(view->GetSize(), 0);
  EXPECT_EQ(view->GetValueSize(), kValueSize);
  EXPECT_FALSE(view->IsModified());

  view.reset();
  EXPECT_EQ(test_render_buffer_.GetModifyCount(), 0);
  EXPECT_EQ(test_render_buffer_.GetInvalidCallCount(), 0);
}

TEST_F(RenderBufferViewTest, GetData) {
  render_buffer.Set(kData, 8);

  auto view = render_buffer.Edit();
  ASSERT_NE(view, nullptr);

  EXPECT_EQ(view->GetSize(), 8);
  EXPECT_EQ(view->GetData(), test_render_buffer_.GetData());
  EXPECT_FALSE(view->IsModified());

  EXPECT_EQ(std::memcmp(view->GetData(3), kData + 3, kValueSize * 5), 0);
  EXPECT_FALSE(view->IsModified());

  view.reset();
  EXPECT_EQ(test_render_buffer_.GetModifyCount(), 1);
  EXPECT_EQ(test_render_buffer_.GetInvalidCallCount(), 0);
}

TEST_F(RenderBufferViewTest, ModifyData) {
  render_buffer.Set(kData, 8);

  auto view = render_buffer.Edit();
  ASSERT_NE(view, nullptr);

  EXPECT_EQ(view->GetSize(), 8);
  EXPECT_EQ(view->ModifyData(), test_render_buffer_.GetData());
  EXPECT_TRUE(view->IsModified());

  EXPECT_EQ(std::memcmp(view->ModifyData(3), kData + 3, kValueSize * 5), 0);
  EXPECT_TRUE(view->IsModified());

  std::memcpy(view->ModifyData(3), kData + 6, kValueSize * 4);
  EXPECT_EQ(std::memcmp(view->GetData(3), kData + 6, kValueSize * 4), 0);
  EXPECT_TRUE(view->IsModified());

  view.reset();
  EXPECT_EQ(test_render_buffer_.GetModifyCount(), 2);
  EXPECT_EQ(test_render_buffer_.GetInvalidCallCount(), 0);
}

TEST_F(RenderBufferViewTest, Resize) {
  render_buffer.Set(kData, 8);

  auto view = render_buffer.Edit();
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(view->GetSize(), 8);
  view->Resize(4);
  EXPECT_FALSE(view->IsModified());
  EXPECT_EQ(view->GetSize(), 4);
  EXPECT_EQ(render_buffer.GetSize(), 8);
  view.reset();
  EXPECT_EQ(test_render_buffer_.GetModifyCount(), 1);
  EXPECT_EQ(render_buffer.GetSize(), 4);
  EXPECT_EQ(std::memcmp(test_render_buffer_.GetData(), kData, 8 * kValueSize),
            0);
  EXPECT_TRUE(test_render_buffer_.CheckBytes(8 * kValueSize, kByteSize, 0xFF));

  view = render_buffer.Edit();
  EXPECT_EQ(view->GetSize(), 4);
  view->Resize(8);
  EXPECT_TRUE(view->IsModified());
  EXPECT_EQ(view->GetSize(), 8);
  EXPECT_EQ(render_buffer.GetSize(), 4);
  view.reset();
  EXPECT_EQ(test_render_buffer_.GetModifyCount(), 2);
  EXPECT_EQ(render_buffer.GetSize(), 8);
  EXPECT_EQ(std::memcmp(test_render_buffer_.GetData(), kData, 8 * kValueSize),
            0);
  EXPECT_TRUE(test_render_buffer_.CheckBytes(8 * kValueSize, kByteSize, 0xFF));

  view.reset();
  EXPECT_EQ(test_render_buffer_.GetInvalidCallCount(), 0);
}

}  // namespace
}  // namespace gb
