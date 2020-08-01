// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_buffer.h"

#include "gb/render/render_buffer_view.h"
#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class RenderBufferTest : public RenderTest {
 protected:
  static constexpr int kValueSize = 4;
  static constexpr int kCapacity = 16;
  static constexpr int kByteSize = kValueSize * kCapacity;
  TestRenderBuffer::Config config;
};

TEST_F(RenderBufferTest, Properties) {
  TestRenderBuffer test_render_buffer(&config, DataVolatility::kStaticWrite,
                                      kValueSize, kCapacity);
  RenderBuffer& render_buffer = test_render_buffer;
  const RenderBuffer& const_render_buffer = render_buffer;

  EXPECT_EQ(const_render_buffer.GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(const_render_buffer.GetValueSize(), kValueSize);
  EXPECT_EQ(const_render_buffer.GetCapacity(), kCapacity);
  EXPECT_EQ(const_render_buffer.GetSize(), 0);
  EXPECT_FALSE(const_render_buffer.IsEditing());

  EXPECT_EQ(test_render_buffer.GetModifyCount(), 0);
  EXPECT_EQ(test_render_buffer.GetInvalidCallCount(), 0);
}

TEST_F(RenderBufferTest, Clear) {
  TestRenderBuffer test_render_buffer(&config, DataVolatility::kStaticWrite,
                                      kValueSize, kCapacity);
  RenderBuffer& render_buffer = test_render_buffer;

  EXPECT_TRUE(render_buffer.Clear());
  EXPECT_EQ(render_buffer.GetSize(), 0);
  EXPECT_TRUE(test_render_buffer.CheckBytes(0, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 0);

  EXPECT_TRUE(render_buffer.Clear(12));
  EXPECT_EQ(render_buffer.GetSize(), 12);
  EXPECT_TRUE(test_render_buffer.CheckBytes(0, 12 * kValueSize, 0));
  EXPECT_TRUE(test_render_buffer.CheckBytes(12 * kValueSize, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 1);

  EXPECT_TRUE(render_buffer.Clear(5));
  EXPECT_EQ(render_buffer.GetSize(), 5);
  EXPECT_TRUE(test_render_buffer.CheckBytes(0, 12 * kValueSize, 0));
  EXPECT_TRUE(test_render_buffer.CheckBytes(12 * kValueSize, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 2);

  EXPECT_TRUE(render_buffer.Clear(kCapacity));
  EXPECT_EQ(render_buffer.GetSize(), kCapacity);
  EXPECT_TRUE(test_render_buffer.CheckBytes(0, kByteSize, 0));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 3);

  config.fail_clear = true;
  EXPECT_FALSE(render_buffer.Clear(8));
  EXPECT_EQ(render_buffer.GetSize(), kCapacity);
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 3);

  EXPECT_EQ(test_render_buffer.GetInvalidCallCount(), 0);
}

TEST_F(RenderBufferTest, Resize) {
  TestRenderBuffer test_render_buffer(&config, DataVolatility::kStaticWrite,
                                      kValueSize, kCapacity);
  RenderBuffer& render_buffer = test_render_buffer;

  EXPECT_TRUE(render_buffer.Resize(0));
  EXPECT_EQ(render_buffer.GetSize(), 0);
  EXPECT_TRUE(test_render_buffer.CheckBytes(0, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 0);

  EXPECT_TRUE(render_buffer.Resize(12));
  EXPECT_EQ(render_buffer.GetSize(), 12);
  EXPECT_TRUE(test_render_buffer.CheckBytes(0, 12 * kValueSize, 0));
  EXPECT_TRUE(test_render_buffer.CheckBytes(12 * kValueSize, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 1);

  test_render_buffer.SetBytes(0, 8 * kValueSize, 0x11);
  test_render_buffer.SetBytes(8 * kValueSize, kByteSize, 0xFF);
  EXPECT_TRUE(render_buffer.Resize(3));
  EXPECT_EQ(render_buffer.GetSize(), 3);
  EXPECT_TRUE(test_render_buffer.CheckBytes(0, 8 * kValueSize, 0x11));
  EXPECT_TRUE(test_render_buffer.CheckBytes(8 * kValueSize, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(),
            1);  // Shrinking is not a modification

  EXPECT_TRUE(render_buffer.Resize(6));
  EXPECT_EQ(render_buffer.GetSize(), 6);
  EXPECT_TRUE(test_render_buffer.CheckBytes(0, 3 * kValueSize, 0x11));
  EXPECT_TRUE(test_render_buffer.CheckBytes(3 * kValueSize, 6 * kValueSize, 0));
  EXPECT_TRUE(
      test_render_buffer.CheckBytes(6 * kValueSize, 8 * kValueSize, 0x11));
  EXPECT_TRUE(test_render_buffer.CheckBytes(8 * kValueSize, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 2);

  config.fail_clear = true;
  EXPECT_FALSE(render_buffer.Resize(8));
  EXPECT_EQ(render_buffer.GetSize(), 6);
  EXPECT_TRUE(test_render_buffer.CheckBytes(0, 3 * kValueSize, 0x11));
  EXPECT_TRUE(test_render_buffer.CheckBytes(3 * kValueSize, 6 * kValueSize, 0));
  EXPECT_TRUE(
      test_render_buffer.CheckBytes(6 * kValueSize, 8 * kValueSize, 0x11));
  EXPECT_TRUE(test_render_buffer.CheckBytes(8 * kValueSize, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 2);

  config.fail_clear = false;
  EXPECT_TRUE(render_buffer.Resize(kCapacity));
  EXPECT_EQ(render_buffer.GetSize(), kCapacity);
  EXPECT_TRUE(test_render_buffer.CheckBytes(0, 3 * kValueSize, 0x11));
  EXPECT_TRUE(test_render_buffer.CheckBytes(3 * kValueSize, kByteSize, 0));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 3);

  EXPECT_EQ(test_render_buffer.GetInvalidCallCount(), 0);
}

TEST_F(RenderBufferTest, Set) {
  const uint32_t kData[kCapacity] = {
      0x00000000, 0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x55555555,
      0x66666666, 0x77777777, 0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB,
      0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF,
  };
  static_assert(sizeof(kData) == kByteSize);

  TestRenderBuffer test_render_buffer(&config, DataVolatility::kStaticWrite,
                                      kValueSize, kCapacity);
  RenderBuffer& render_buffer = test_render_buffer;

  EXPECT_TRUE(render_buffer.Set(nullptr, 0));
  EXPECT_EQ(render_buffer.GetSize(), 0);

  EXPECT_TRUE(render_buffer.Set(kData, 8));
  EXPECT_EQ(render_buffer.GetSize(), 8);
  EXPECT_EQ(
      std::memcmp(kData, test_render_buffer.GetData(), 8 * sizeof(kValueSize)),
      0);
  EXPECT_TRUE(test_render_buffer.CheckBytes(8 * kValueSize, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 1);

  EXPECT_TRUE(render_buffer.Set(kData + 8, 4));
  EXPECT_EQ(render_buffer.GetSize(), 4);
  EXPECT_EQ(std::memcmp(kData + 8, test_render_buffer.GetData(),
                        4 * sizeof(kValueSize)),
            0);
  EXPECT_EQ(
      std::memcmp(kData + 4,
                  static_cast<uint32_t*>(test_render_buffer.GetData()) + 4,
                  4 * sizeof(kValueSize)),
      0);
  EXPECT_TRUE(test_render_buffer.CheckBytes(8 * kValueSize, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 2);

  config.fail_set = true;
  EXPECT_FALSE(render_buffer.Set(kData, kCapacity));
  EXPECT_EQ(render_buffer.GetSize(), 4);
  EXPECT_EQ(std::memcmp(kData + 8, test_render_buffer.GetData(),
                        4 * sizeof(kValueSize)),
            0);
  EXPECT_EQ(
      std::memcmp(kData + 4,
                  static_cast<uint32_t*>(test_render_buffer.GetData()) + 4,
                  4 * sizeof(kValueSize)),
      0);
  EXPECT_TRUE(test_render_buffer.CheckBytes(8 * kValueSize, kByteSize, 0xFF));
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 2);

  config.fail_set = false;
  EXPECT_TRUE(render_buffer.Set(kData, kCapacity));
  EXPECT_EQ(render_buffer.GetSize(), kCapacity);
  EXPECT_EQ(std::memcmp(kData, test_render_buffer.GetData(), sizeof(kData)), 0);
  EXPECT_EQ(test_render_buffer.GetModifyCount(), 3);

  EXPECT_EQ(test_render_buffer.GetInvalidCallCount(), 0);
}

TEST_F(RenderBufferTest, EditPreventsModification) {
  TestRenderBuffer test_render_buffer(&config, DataVolatility::kStaticReadWrite,
                                      kValueSize, kCapacity);
  RenderBuffer& render_buffer = test_render_buffer;

  auto view = render_buffer.Edit();
  ASSERT_NE(view, nullptr);

  EXPECT_FALSE(render_buffer.Clear(kCapacity));
  EXPECT_EQ(render_buffer.GetSize(), 0);
  EXPECT_FALSE(render_buffer.Resize(kCapacity));
  EXPECT_EQ(render_buffer.GetSize(), 0);
  uint32_t data = 0x12345678;
  EXPECT_FALSE(render_buffer.Set(&data, 4));
  EXPECT_EQ(render_buffer.GetSize(), 0);
  EXPECT_EQ(render_buffer.Edit(), nullptr);
  view.reset();

  EXPECT_EQ(test_render_buffer.GetModifyCount(), 0);
  EXPECT_EQ(test_render_buffer.GetInvalidCallCount(), 0);
}

TEST_F(RenderBufferTest, CannotEditStaticWrite) {
  TestRenderBuffer test_render_buffer(&config, DataVolatility::kStaticWrite,
                                      kValueSize, kCapacity);
  RenderBuffer& render_buffer = test_render_buffer;

  auto view = render_buffer.Edit();
  ASSERT_EQ(view, nullptr);

  EXPECT_EQ(test_render_buffer.GetModifyCount(), 0);
  EXPECT_EQ(test_render_buffer.GetInvalidCallCount(), 0);
}

TEST_F(RenderBufferTest, FailEdit) {
  TestRenderBuffer test_render_buffer(&config, DataVolatility::kStaticReadWrite,
                                      kValueSize, kCapacity);
  RenderBuffer& render_buffer = test_render_buffer;

  config.fail_edit_begin = true;
  auto view = render_buffer.Edit();
  ASSERT_EQ(view, nullptr);

  EXPECT_EQ(test_render_buffer.GetModifyCount(), 0);
  EXPECT_EQ(test_render_buffer.GetInvalidCallCount(), 0);
}

}  // namespace
}  // namespace gb
