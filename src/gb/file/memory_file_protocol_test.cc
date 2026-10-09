// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/memory_file_protocol.h"

#include "absl/strings/str_cat.h"
#include "gb/file/common_protocol_test.h"
#include "gb/file/file_system.h"
#include "gb/file/raw_file.h"
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
  EXPECT_EQ(protocol.GetFlags(), kReadWriteFileProtocolFlags);
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

TEST(MemoryFileProtocolTest, CreateFolderUnderMissingHost) {
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::make_unique<MemoryFileProtocol>()));
  EXPECT_FALSE(
      file_system.CreateFolder("mem://host/a", FolderMode::kRecursive));
}

TEST(MemoryFileProtocolTest, ResetFileHasNoSize) {
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::make_unique<MemoryFileProtocol>()));
  ASSERT_TRUE(file_system.WriteFile("mem:/file", "12345"));
  auto file =
      file_system.OpenFile("mem:/file", {FileFlag::kWrite, FileFlag::kReset});
  ASSERT_NE(file, nullptr);
  file.reset();
  EXPECT_EQ(file_system.GetPathInfo("mem:/file").size, 0);
}

TEST(MemoryFileProtocolTest, CopyMissingFolderAcrossProtocols) {
  FileSystem file_system;
  ASSERT_TRUE(
      file_system.Register(std::make_unique<MemoryFileProtocol>(), "a"));
  ASSERT_TRUE(
      file_system.Register(std::make_unique<MemoryFileProtocol>(), "b"));
  ASSERT_TRUE(file_system.WriteFile("a:/file", "12345"));
  EXPECT_FALSE(file_system.CopyFolder("a:/missing", "b:/missing"));
  EXPECT_EQ(file_system.GetPathInfo("b:/missing").type, PathType::kInvalid);
  EXPECT_FALSE(file_system.CopyFolder("a:/file", "b:/file"));
  EXPECT_EQ(file_system.GetPathInfo("b:/file").type, PathType::kInvalid);
}

TEST(MemoryFileProtocolTest, CopyWithinProtocolWithTwoNames) {
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::make_unique<MemoryFileProtocol>(),
                                   std::vector<std::string>{"a", "b"}));
  ASSERT_TRUE(file_system.CreateFolder("a:/folder"));
  ASSERT_TRUE(file_system.WriteFile("a:/folder/file", "12345"));

  // The same file under both names isn't truncated by copying it to itself.
  file_system.CopyFile("a:/folder/file", "b:/folder/file");
  std::string contents;
  ASSERT_TRUE(file_system.ReadFile("a:/folder/file", &contents));
  EXPECT_EQ(contents, "12345");

  // A folder can't be copied into itself, under either name.
  EXPECT_FALSE(file_system.CopyFolder("a:/folder", "b:/folder/sub"));

  // Copies under the other name work as they do under one name.
  EXPECT_TRUE(file_system.CopyFile("a:/folder/file", "b:/copy"));
  ASSERT_TRUE(file_system.ReadFile("a:/copy", &contents));
  EXPECT_EQ(contents, "12345");
}

TEST(MemoryFileProtocolTest, RelativePathsAreRejected) {
  MemoryFileProtocol protocol;
  EXPECT_EQ(protocol.GetPathInfo("mem", "").type, PathType::kInvalid);
  EXPECT_FALSE(protocol.CreateFolder("mem", "folder", FolderMode::kNormal));
  EXPECT_EQ(protocol.OpenFile("mem", "file", kNewFileFlags), nullptr);
  EXPECT_THAT(protocol.List("mem", "", {}, FolderMode::kRecursive,
                            {PathType::kFile, PathType::kFolder}),
              IsEmpty());
}

}  // namespace
}  // namespace gb
