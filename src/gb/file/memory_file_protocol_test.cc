// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/memory_file_protocol.h"

#include "absl/strings/str_cat.h"
#include "gb/file/common_protocol_test.h"
#include "gb/file/file_system.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Values;

std::unique_ptr<FileProtocol> MemoryFileProtocolFactory(
    const CommonProtocolTestInit& init) {
  return init.DefaultInit(std::make_unique<MemoryFileProtocol>());
}

INSTANTIATE_TEST_SUITE_P(MemoryFileProtocolTest, CommonProtocolTest,
                         Values(MemoryFileProtocolFactory));

TEST(MemoryFileProtocolTest, Construct) {
  MemoryFileProtocol protocol;
  EXPECT_EQ(protocol.GetFlags(), kAllFileProtocolFlags);
  EXPECT_THAT(protocol.GetDefaultNames(), ElementsAre("mem"));

  const FileProtocolFlags flags = {FileProtocolFlag::kInfo,
                                   FileProtocolFlag::kFileRead};
  EXPECT_EQ(MemoryFileProtocol(flags).GetFlags(), flags);
}

TEST(MemoryFileProtocolTest, DeleteOpenFile) {
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::make_unique<MemoryFileProtocol>()));
  auto file = file_system.OpenFile("mem:/file", kNewFileFlags);
  EXPECT_NE(file, nullptr);
  EXPECT_FALSE(file_system.DeleteFile("mem:/file"));
  EXPECT_EQ(file_system.GetPathInfo("mem:/file").type, PathType::kFile);
  file.reset();
  EXPECT_TRUE(file_system.DeleteFile("mem:/file"));
  EXPECT_EQ(file_system.GetPathInfo("mem:/file").type, PathType::kInvalid);
}

TEST(MemoryFileProtocolTest, OpenAnOpenFile) {
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::make_unique<MemoryFileProtocol>()));
  auto file = file_system.OpenFile("mem:/file", kNewFileFlags);
  EXPECT_NE(file, nullptr);
  EXPECT_EQ(file_system.OpenFile("mem:/file", kReadFileFlags), nullptr);
}

TEST(MemoryFileProtocolTest, FileInvalidAfterClose) {
  auto file_system = std::make_unique<FileSystem>();
  ASSERT_TRUE(file_system->Register(std::make_unique<MemoryFileProtocol>()));

  std::unique_ptr<File> files[4] = {};
  int i = 0;
  for (auto& file : files) {
    file = file_system->OpenFile(absl::StrCat("mem:/file-", i),
                                 kNewFileFlags + FileFlag::kRead);
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(file->WriteString("1234567890"), 10);
    EXPECT_EQ(file->SeekBegin(), 0);
    ++i;
  }
  file_system.reset();

  EXPECT_EQ(files[0]->SeekEnd(), -1);
  EXPECT_EQ(files[1]->SeekTo(5), -1);
  EXPECT_THAT(files[2]->ReadString(10), IsEmpty());
  EXPECT_EQ(files[3]->WriteString("abcdefghij"), 0);
}

}  // namespace
}  // namespace gb
