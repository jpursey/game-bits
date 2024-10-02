// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/source_file.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(SourceFileTest, FromText) {
  auto source_file = SourceFile::FromText("Hello\nWorld\n");
  ASSERT_NE(source_file, nullptr);
  EXPECT_THAT(source_file->GetFilename(), IsEmpty());
  EXPECT_EQ(source_file->GetContent(), "Hello\nWorld\n");
  EXPECT_THAT(source_file->GetLines(), ElementsAre("Hello", "World"));
}

TEST(SourceFileTest, FromFileText) {
  auto source_file = SourceFile::FromFileText("test.txt", "Hello\nWorld\n");
  ASSERT_NE(source_file, nullptr);
  EXPECT_EQ(source_file->GetFilename(), "test.txt");
  EXPECT_EQ(source_file->GetContent(), "Hello\nWorld\n");
  EXPECT_THAT(source_file->GetLines(), ElementsAre("Hello", "World"));
}

}  // namespace
}  // namespace gb
