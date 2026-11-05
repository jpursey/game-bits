// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/storage_buffer.h"

#include <vector>

#include "gb/render/render_test.h"
#include "gb/render/test_render_buffer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;

constexpr int kCount = 8;

class StorageBufferTest : public RenderTest {
 protected:
  // Creates the system, and registers Vector2 and Vector3 as constants types.
  void CreateSystemWithTypes() {
    CreateSystem();
    vector2_type_ = render_system_->RegisterConstantsType<Vector2>("vec2");
    vector3_type_ = render_system_->RegisterConstantsType<Vector3>("vec3");
    ASSERT_NE(vector2_type_, nullptr);
    ASSERT_NE(vector3_type_, nullptr);
  }

  // Creates a storage buffer of kCount Vector2 values.
  ResourcePtr<StorageBuffer> CreateBuffer(
      DataVolatility volatility = DataVolatility::kStaticReadWrite) {
    return render_system_->CreateStorageBuffer(vector2_type_, volatility,
                                               kCount);
  }

  // Returns the values of a Vector2 storage buffer, read through the test
  // render buffer, so it works for any volatility.
  std::vector<Vector2> GetValues(StorageBuffer* storage_buffer) {
    auto* buffer = static_cast<TestRenderBuffer*>(
        storage_buffer->GetBuffer(GetAccessToken()));
    const auto* values = static_cast<const Vector2*>(buffer->GetData());
    return std::vector<Vector2>(values, values + buffer->GetSize());
  }

  const RenderDataType* vector2_type_ = nullptr;
  const RenderDataType* vector3_type_ = nullptr;
};

TEST_F(StorageBufferTest, CreateAsResourcePtr) {
  CreateSystemWithTypes();
  ResourcePtr<StorageBuffer> storage_buffer = CreateBuffer();
  ASSERT_NE(storage_buffer, nullptr);
  auto* buffer = static_cast<TestRenderBuffer*>(
      storage_buffer->GetBuffer(GetAccessToken()));
  EXPECT_EQ(buffer->GetInvalidCallCount(), 0);
  EXPECT_EQ(buffer->GetValueSize(), static_cast<int>(sizeof(Vector2)));
  EXPECT_EQ(buffer->GetCapacity(), kCount);
}

TEST_F(StorageBufferTest, CreateInResourceSet) {
  CreateSystemWithTypes();
  ResourceSet resource_set;
  StorageBuffer* storage_buffer = render_system_->CreateStorageBuffer(
      &resource_set, vector2_type_, DataVolatility::kStaticWrite, kCount);
  ASSERT_NE(storage_buffer, nullptr);
  EXPECT_EQ(resource_set.Get<StorageBuffer>(storage_buffer->GetResourceId()),
            storage_buffer);
}

TEST_F(StorageBufferTest, Properties) {
  CreateSystemWithTypes();
  auto storage_buffer = CreateBuffer(DataVolatility::kPerFrame);
  ASSERT_NE(storage_buffer, nullptr);
  EXPECT_EQ(storage_buffer->GetVolatility(), DataVolatility::kPerFrame);
  EXPECT_EQ(storage_buffer->GetType(), vector2_type_);
  EXPECT_EQ(storage_buffer->GetCount(), kCount);
}

TEST_F(StorageBufferTest, CreateIsZero) {
  CreateSystemWithTypes();
  for (DataVolatility volatility :
       {DataVolatility::kStaticWrite, DataVolatility::kStaticReadWrite,
        DataVolatility::kPerFrame}) {
    auto storage_buffer = CreateBuffer(volatility);
    ASSERT_NE(storage_buffer, nullptr);
    EXPECT_THAT(GetValues(storage_buffer.Get()), Each(Vector2{0, 0}));
  }
}

TEST_F(StorageBufferTest, CreateValidatesTypeAndCount) {
  CreateSystemWithTypes();
  EXPECT_EQ(render_system_->CreateStorageBuffer(
                nullptr, DataVolatility::kStaticReadWrite, kCount),
            nullptr);
  EXPECT_EQ(render_system_->CreateStorageBuffer(
                vector2_type_, DataVolatility::kStaticReadWrite, 0),
            nullptr);
  EXPECT_EQ(render_system_->CreateStorageBuffer(
                vector2_type_, DataVolatility::kStaticReadWrite, -1),
            nullptr);

  const int max_count =
      kMaxStorageBufferBytes / static_cast<int>(sizeof(Vector2));
  EXPECT_NE(render_system_->CreateStorageBuffer(
                vector2_type_, DataVolatility::kStaticWrite, max_count),
            nullptr);
  EXPECT_EQ(render_system_->CreateStorageBuffer(
                vector2_type_, DataVolatility::kStaticWrite, max_count + 1),
            nullptr);
}

TEST_F(StorageBufferTest, FailCreate) {
  CreateSystemWithTypes();
  state_.fail_create_storage_buffer = true;
  EXPECT_EQ(CreateBuffer(), nullptr);
  state_.fail_create_storage_buffer = false;
  state_.storage_buffer_config.fail_clear = true;
  EXPECT_EQ(CreateBuffer(), nullptr);
}

TEST_F(StorageBufferTest, SetAndGet) {
  CreateSystemWithTypes();
  auto storage_buffer = CreateBuffer();
  ASSERT_NE(storage_buffer, nullptr);

  const std::vector<Vector2> values = {{1, 2}, {3, 4}, {5, 6}};
  EXPECT_TRUE(storage_buffer->Set<Vector2>(2, values));
  EXPECT_THAT(
      GetValues(storage_buffer.Get()),
      ElementsAre(Vector2{0, 0}, Vector2{0, 0}, Vector2{1, 2}, Vector2{3, 4},
                  Vector2{5, 6}, Vector2{0, 0}, Vector2{0, 0}, Vector2{0, 0}));

  std::vector<Vector2> read(4, Vector2{-1, -1});
  EXPECT_TRUE(storage_buffer->Get<Vector2>(1, absl::MakeSpan(read)));
  EXPECT_THAT(read, ElementsAre(Vector2{0, 0}, Vector2{1, 2}, Vector2{3, 4},
                                Vector2{5, 6}));

  // The last value, and no values at all.
  EXPECT_TRUE(storage_buffer->Set<Vector2>(kCount - 1, {{7, 8}}));
  EXPECT_TRUE(storage_buffer->Get<Vector2>(kCount - 1,
                                           absl::MakeSpan(read).subspan(0, 1)));
  EXPECT_EQ(read[0], (Vector2{7, 8}));
  EXPECT_TRUE(storage_buffer->Set<Vector2>(kCount, {}));
  EXPECT_TRUE(storage_buffer->Get<Vector2>(kCount, {}));
}

TEST_F(StorageBufferTest, SetAndGetValidateRange) {
  CreateSystemWithTypes();
  auto storage_buffer = CreateBuffer();
  ASSERT_NE(storage_buffer, nullptr);

  std::vector<Vector2> values(3, Vector2{1, 2});
  EXPECT_FALSE(storage_buffer->Set<Vector2>(kCount - 2, values));
  EXPECT_FALSE(storage_buffer->Set<Vector2>(-1, values));
  EXPECT_FALSE(storage_buffer->Set<Vector2>(kCount + 1, {}));
  EXPECT_FALSE(
      storage_buffer->Get<Vector2>(kCount - 2, absl::MakeSpan(values)));
  EXPECT_FALSE(storage_buffer->Get<Vector2>(-1, absl::MakeSpan(values)));
  EXPECT_THAT(GetValues(storage_buffer.Get()), Each(Vector2{0, 0}));
}

TEST_F(StorageBufferTest, SetAndGetValidateType) {
  CreateSystemWithTypes();
  auto storage_buffer = CreateBuffer();
  ASSERT_NE(storage_buffer, nullptr);

  std::vector<Vector3> values(1, Vector3{1, 2, 3});
  EXPECT_FALSE(storage_buffer->Set<Vector3>(0, values));
  EXPECT_FALSE(storage_buffer->Get<Vector3>(0, absl::MakeSpan(values)));
  EXPECT_THAT(GetValues(storage_buffer.Get()), Each(Vector2{0, 0}));
}

TEST_F(StorageBufferTest, Clear) {
  CreateSystemWithTypes();
  auto storage_buffer = CreateBuffer();
  ASSERT_NE(storage_buffer, nullptr);
  ASSERT_TRUE(storage_buffer->Set<Vector2>(
      0, std::vector<Vector2>(kCount, Vector2{1, 2})));

  EXPECT_TRUE(storage_buffer->Clear());
  EXPECT_EQ(storage_buffer->GetCount(), kCount);
  EXPECT_THAT(GetValues(storage_buffer.Get()), Each(Vector2{0, 0}));
}

TEST_F(StorageBufferTest, CannotGetStaticWrite) {
  CreateSystemWithTypes();
  auto storage_buffer = CreateBuffer(DataVolatility::kStaticWrite);
  ASSERT_NE(storage_buffer, nullptr);
  EXPECT_TRUE(storage_buffer->Set<Vector2>(0, {{1, 2}}));

  std::vector<Vector2> read(1);
  EXPECT_FALSE(storage_buffer->Get<Vector2>(0, absl::MakeSpan(read)));
}

TEST_F(StorageBufferTest, BackendFailures) {
  CreateSystemWithTypes();
  auto storage_buffer = CreateBuffer();
  ASSERT_NE(storage_buffer, nullptr);

  state_.storage_buffer_config.fail_set = true;
  EXPECT_FALSE(storage_buffer->Set<Vector2>(0, {{1, 2}}));
  state_.storage_buffer_config.fail_clear = true;
  EXPECT_FALSE(storage_buffer->Clear());
  state_.storage_buffer_config.fail_get = true;
  std::vector<Vector2> read(1);
  EXPECT_FALSE(storage_buffer->Get<Vector2>(0, absl::MakeSpan(read)));
}

}  // namespace
}  // namespace gb
