// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/common_protocol_test.h"

#include "gb/file/file_system.h"
#include "gb/file/raw_file.h"
#include "gb/test/test_util.h"
#include "gmock/gmock.h"

namespace gb {

std::unique_ptr<FileProtocol> CommonProtocolTestInit::DefaultInit(
    std::unique_ptr<FileProtocol> protocol) const {
  for (const auto& path : folders) {
    if (!protocol->CreateFolder("test", path, FolderMode::kNormal)) {
      return {};
    }
  }
  for (const auto& [path, contents] : files) {
    auto file = protocol->OpenFile("test", path, kNewFileFlags);
    if (file == nullptr) {
      return {};
    }
    const int64_t size = static_cast<int64_t>(contents.size());
    if (file->Write(contents.data(), size) != size) {
      return {};
    }
  }
  return protocol;
}

CommonProtocolTest::CommonProtocolTest() {}

namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST_P(CommonProtocolTest, EmptyRootFolder) {
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kInfo)) {
    return;
  }
  EXPECT_EQ(file_system.GetPathInfo("test:/").type, PathType::kFolder);
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kList)) {
    return;
  }
  EXPECT_THAT(file_system.List("test:/"), IsEmpty());
}

TEST_P(CommonProtocolTest, GetPathInfo) {
  CommonProtocolTestInit init;
  init.folders = {"/folder-1", "/folder-1/sub-1"};
  init.files = {{"/file-1", "1234567890"}, {"/folder-1/file-2", "abcdefghij"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kInfo)) {
    return;
  }
  file_system.SetDefaultProtocol("test");
  for (const auto& path : init.folders) {
    auto info = file_system.GetPathInfo(path);
    EXPECT_EQ(info.type, PathType::kFolder);
    EXPECT_EQ(info.size, 0);
  }
  for (const auto& [path, contents] : init.files) {
    auto info = file_system.GetPathInfo(path);
    EXPECT_EQ(info.type, PathType::kFile);
    EXPECT_EQ(info.size, static_cast<int64_t>(contents.size()));
  }
}

TEST_P(CommonProtocolTest, List) {
  CommonProtocolTestInit init;
  init.folders = {"/folder-1", "/folder-1/sub-folder-1",
                  "/folder-1/sub-folder-2", "/folder-2",
                  "/folder-2/sub-folder-3"};
  init.files = {{"/file-1", "1234567890"},
                {"/file-2", "abcdefghij"},
                {"/folder-1/sub-folder-1/file-3", "0987654321"},
                {"/folder-1/sub-folder-1/file-4", "klmnopqrst"},
                {"/folder-2/file-5", "testing is a good thing."}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kList)) {
    return;
  }
  EXPECT_THAT(file_system.List("test:/invalid", "", FolderMode::kNormal),
              IsEmpty());
  EXPECT_THAT(file_system.List("test:/invalid", "", FolderMode::kRecursive),
              IsEmpty());
  EXPECT_THAT(file_system.List("test:/file-1", "", FolderMode::kNormal),
              IsEmpty());
  EXPECT_THAT(file_system.List("test:/file-1", "", FolderMode::kRecursive),
              IsEmpty());
  EXPECT_THAT(file_system.List("test:/", "", FolderMode::kNormal),
              UnorderedElementsAre("test:/folder-1", "test:/folder-2",
                                   "test:/file-1", "test:/file-2"));
  EXPECT_THAT(file_system.List("test:/", "*", FolderMode::kNormal),
              UnorderedElementsAre("test:/folder-1", "test:/folder-2",
                                   "test:/file-1", "test:/file-2"));
  EXPECT_THAT(file_system.List("test:/", "f*-1", FolderMode::kNormal),
              UnorderedElementsAre("test:/folder-1", "test:/file-1"));
  EXPECT_THAT(
      file_system.List("test:/", "", FolderMode::kRecursive),
      UnorderedElementsAre("test:/folder-1", "test:/folder-1/sub-folder-1",
                           "test:/folder-1/sub-folder-2", "test:/folder-2",
                           "test:/folder-2/sub-folder-3", "test:/file-1",
                           "test:/file-2", "test:/folder-1/sub-folder-1/file-3",
                           "test:/folder-1/sub-folder-1/file-4",
                           "test:/folder-2/file-5"));
  EXPECT_THAT(
      file_system.List("test:/", "*", FolderMode::kRecursive),
      UnorderedElementsAre("test:/folder-1", "test:/folder-1/sub-folder-1",
                           "test:/folder-1/sub-folder-2", "test:/folder-2",
                           "test:/folder-2/sub-folder-3", "test:/file-1",
                           "test:/file-2", "test:/folder-1/sub-folder-1/file-3",
                           "test:/folder-1/sub-folder-1/file-4",
                           "test:/folder-2/file-5"));
  EXPECT_THAT(file_system.List("test:/", "*-2", FolderMode::kRecursive),
              UnorderedElementsAre("test:/folder-2", "test:/file-2",
                                   "test:/folder-1/sub-folder-2"));
  EXPECT_THAT(file_system.List("test:/folder-1", "", FolderMode::kRecursive),
              UnorderedElementsAre("test:/folder-1/sub-folder-1",
                                   "test:/folder-1/sub-folder-2",
                                   "test:/folder-1/sub-folder-1/file-3",
                                   "test:/folder-1/sub-folder-1/file-4"));
}

TEST_P(CommonProtocolTest, ListFolders) {
  CommonProtocolTestInit init;
  init.folders = {"/folder-1", "/folder-1/sub-folder-1",
                  "/folder-1/sub-folder-2", "/folder-2",
                  "/folder-2/sub-folder-3"};
  init.files = {{"/file-1", "1234567890"},
                {"/file-2", "abcdefghij"},
                {"/folder-1/sub-folder-1/file-3", "0987654321"},
                {"/folder-1/sub-folder-1/file-4", "klmnopqrst"},
                {"/folder-2/file-5", "testing is a good thing."}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kList)) {
    return;
  }
  EXPECT_THAT(file_system.ListFolders("test:/invalid", "", FolderMode::kNormal),
              IsEmpty());
  EXPECT_THAT(file_system.ListFolders("test:/file-1", "", FolderMode::kNormal),
              IsEmpty());
  EXPECT_THAT(file_system.ListFolders("test:/", "", FolderMode::kNormal),
              UnorderedElementsAre("test:/folder-1", "test:/folder-2"));
  EXPECT_THAT(file_system.ListFolders("test:/", "*", FolderMode::kNormal),
              UnorderedElementsAre("test:/folder-1", "test:/folder-2"));
  EXPECT_THAT(file_system.ListFolders("test:/", "f*-1", FolderMode::kNormal),
              UnorderedElementsAre("test:/folder-1"));
  EXPECT_THAT(
      file_system.ListFolders("test:/", "", FolderMode::kRecursive),
      UnorderedElementsAre("test:/folder-1", "test:/folder-1/sub-folder-1",
                           "test:/folder-1/sub-folder-2", "test:/folder-2",
                           "test:/folder-2/sub-folder-3"));
  EXPECT_THAT(
      file_system.ListFolders("test:/", "*", FolderMode::kRecursive),
      UnorderedElementsAre("test:/folder-1", "test:/folder-1/sub-folder-1",
                           "test:/folder-1/sub-folder-2", "test:/folder-2",
                           "test:/folder-2/sub-folder-3"));
  EXPECT_THAT(
      file_system.ListFolders("test:/", "*-2", FolderMode::kRecursive),
      UnorderedElementsAre("test:/folder-2", "test:/folder-1/sub-folder-2"));
  EXPECT_THAT(
      file_system.ListFolders("test:/folder-1", "", FolderMode::kRecursive),
      UnorderedElementsAre("test:/folder-1/sub-folder-1",
                           "test:/folder-1/sub-folder-2"));
}

TEST_P(CommonProtocolTest, ListFiles) {
  CommonProtocolTestInit init;
  init.folders = {"/folder-1", "/folder-1/sub-folder-1",
                  "/folder-1/sub-folder-2", "/folder-2",
                  "/folder-2/sub-folder-3"};
  init.files = {{"/file-1", "1234567890"},
                {"/file-2", "abcdefghij"},
                {"/folder-1/sub-folder-1/file-3", "0987654321"},
                {"/folder-1/sub-folder-1/file-4", "klmnopqrst"},
                {"/folder-2/file-5", "testing is a good thing."}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kList)) {
    return;
  }
  EXPECT_THAT(file_system.ListFiles("test:/invalid", "", FolderMode::kNormal),
              IsEmpty());
  EXPECT_THAT(file_system.ListFiles("test:/file-1", "", FolderMode::kNormal),
              IsEmpty());
  EXPECT_THAT(file_system.ListFiles("test:/", "", FolderMode::kNormal),
              UnorderedElementsAre("test:/file-1", "test:/file-2"));
  EXPECT_THAT(file_system.ListFiles("test:/", "*", FolderMode::kNormal),
              UnorderedElementsAre("test:/file-1", "test:/file-2"));
  EXPECT_THAT(file_system.ListFiles("test:/", "f*-1", FolderMode::kNormal),
              UnorderedElementsAre("test:/file-1"));
  EXPECT_THAT(file_system.ListFiles("test:/", "", FolderMode::kRecursive),
              UnorderedElementsAre("test:/file-1", "test:/file-2",
                                   "test:/folder-1/sub-folder-1/file-3",
                                   "test:/folder-1/sub-folder-1/file-4",
                                   "test:/folder-2/file-5"));
  EXPECT_THAT(file_system.ListFiles("test:/", "*", FolderMode::kRecursive),
              UnorderedElementsAre("test:/file-1", "test:/file-2",
                                   "test:/folder-1/sub-folder-1/file-3",
                                   "test:/folder-1/sub-folder-1/file-4",
                                   "test:/folder-2/file-5"));
  EXPECT_THAT(file_system.ListFiles("test:/", "f*-3", FolderMode::kRecursive),
              UnorderedElementsAre("test:/folder-1/sub-folder-1/file-3"));
  EXPECT_THAT(
      file_system.ListFiles("test:/folder-1", "", FolderMode::kRecursive),
      UnorderedElementsAre("test:/folder-1/sub-folder-1/file-3",
                           "test:/folder-1/sub-folder-1/file-4"));
}

TEST_P(CommonProtocolTest, CreateFolder) {
  CommonProtocolTestInit init;
  init.files = {{"/file-1", "1234567890"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kFolderCreate)) {
    return;
  }
  const bool check_info =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kInfo);
  EXPECT_TRUE(file_system.CreateFolder("test:/", FolderMode::kNormal));
  EXPECT_TRUE(file_system.CreateFolder("test:/folder-1", FolderMode::kNormal));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-1").type ==
                                 PathType::kFolder);
  EXPECT_TRUE(file_system.CreateFolder("test:/folder-1", FolderMode::kNormal));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-1").type ==
                                 PathType::kFolder);
  EXPECT_FALSE(file_system.CreateFolder("test:/file-1", FolderMode::kNormal));
  EXPECT_TRUE(!check_info ||
              file_system.GetPathInfo("test:/file-1").type == PathType::kFile);
  EXPECT_FALSE(
      file_system.CreateFolder("test:/folder-2/folder-3", FolderMode::kNormal));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-2").type ==
                                 PathType::kInvalid);
  EXPECT_TRUE(file_system.CreateFolder("test:/folder-4/folder-5",
                                       FolderMode::kRecursive));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-4").type ==
                                 PathType::kFolder);
  EXPECT_TRUE(!check_info ||
              file_system.GetPathInfo("test:/folder-4/folder-5").type ==
                  PathType::kFolder);
  EXPECT_TRUE(file_system.CreateFolder("test:/folder-1/folder-6/folder-7",
                                       FolderMode::kRecursive));
  EXPECT_TRUE(!check_info ||
              file_system.GetPathInfo("test:/folder-1/folder-6").type ==
                  PathType::kFolder);
  EXPECT_TRUE(
      !check_info ||
      file_system.GetPathInfo("test:/folder-1/folder-6/folder-7").type ==
          PathType::kFolder);
}

TEST_P(CommonProtocolTest, DeleteFolder) {
  CommonProtocolTestInit init;
  init.folders = {"/empty-1",          "/folder-1",
                  "/folder-1/empty-2", "/folder-2",
                  "/folder-3",         "/folder-3/sub-1",
                  "/folder-3/sub-2",   "/folder-3/sub-1/sub-3"};
  init.files = {{"/file-1", "1234567890"},
                {"/folder-2/file-2", "AAAAA"},
                {"/folder-3/file-3", "BBBBB"},
                {"/folder-3/file-4", "CCCCC"},
                {"/folder-3/sub-1/file-5", "DDDDD"},
                {"/folder-3/sub-1/sub-3/file-6", "EEEEE"},
                {"/folder-3/sub-2/file-7", "FFFFF"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kFolderCreate)) {
    return;
  }
  const bool check_info =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kInfo);
  EXPECT_FALSE(file_system.DeleteFolder("test:/", FolderMode::kNormal));
  EXPECT_TRUE(!check_info ||
              file_system.GetPathInfo("test:/").type == PathType::kFolder);
  EXPECT_TRUE(file_system.DeleteFolder("test:/invalid", FolderMode::kNormal));
  EXPECT_FALSE(file_system.DeleteFolder("test:/file-1", FolderMode::kNormal));
  EXPECT_TRUE(!check_info ||
              file_system.GetPathInfo("test:/file-1").type == PathType::kFile);
  EXPECT_FALSE(file_system.DeleteFolder("test:/folder-1", FolderMode::kNormal));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-1").type ==
                                 PathType::kFolder);
  EXPECT_FALSE(file_system.DeleteFolder("test:/folder-2", FolderMode::kNormal));
  EXPECT_TRUE(!check_info ||
              file_system.GetPathInfo("test:/").type == PathType::kFolder);
  EXPECT_TRUE(file_system.DeleteFolder("test:/empty-1", FolderMode::kNormal));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/empty-1").type ==
                                 PathType::kInvalid);
  EXPECT_TRUE(file_system.DeleteFolder("test:/empty-1", FolderMode::kNormal));
  EXPECT_TRUE(
      file_system.DeleteFolder("test:/folder-1/empty-2", FolderMode::kNormal));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-1").type ==
                                 PathType::kFolder);
  EXPECT_TRUE(!check_info ||
              file_system.GetPathInfo("test:/folder-1/empty-2").type ==
                  PathType::kInvalid);
  EXPECT_TRUE(file_system.DeleteFolder("test:/folder-1", FolderMode::kNormal));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-1").type ==
                                 PathType::kInvalid);

  EXPECT_FALSE(file_system.DeleteFolder("test:/", FolderMode::kRecursive));
  EXPECT_TRUE(!check_info ||
              file_system.GetPathInfo("test:/").type == PathType::kFolder);
  EXPECT_TRUE(
      file_system.DeleteFolder("test:/folder-2", FolderMode::kRecursive));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-2").type ==
                                 PathType::kInvalid);
  EXPECT_TRUE(
      file_system.DeleteFolder("test:/folder-3/sub-2", FolderMode::kRecursive));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-3").type ==
                                 PathType::kFolder);
  EXPECT_TRUE(!check_info ||
              file_system.GetPathInfo("test:/folder-3/sub-2").type ==
                  PathType::kInvalid);
  EXPECT_TRUE(
      file_system.DeleteFolder("test:/folder-3", FolderMode::kRecursive));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-3").type ==
                                 PathType::kInvalid);
}

TEST_P(CommonProtocolTest, DeleteFile) {
  CommonProtocolTestInit init;
  init.folders = {"/folder-1"};
  init.files = {{"/file-1", "1234567890"}, {"/folder-1/file-2", "abcdefghij"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileCreate)) {
    return;
  }
  const bool check_info =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kInfo);
  EXPECT_TRUE(file_system.DeleteFile("test:/invalid"));
  EXPECT_FALSE(file_system.DeleteFile("test:/folder-1"));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/folder-1").type ==
                                 PathType::kFolder);
  EXPECT_TRUE(file_system.DeleteFile("test:/file-1"));
  EXPECT_TRUE(!check_info || file_system.GetPathInfo("test:/file-1").type ==
                                 PathType::kInvalid);
  EXPECT_TRUE(file_system.DeleteFile("test:/folder-1/file-2"));
  EXPECT_TRUE(!check_info ||
              file_system.GetPathInfo("test:/folder-1/file-2").type ==
                  PathType::kInvalid);
}

bool CheckContents(FileSystem& file_system, std::string_view path,
                   const std::string& expected_contents) {
  auto file = file_system.OpenFile(path, kReadFileFlags);
  EXPECT_NE(file, nullptr);
  if (file == nullptr) {
    return false;
  }
  std::string contents;
  file->ReadRemainingString(&contents);
  EXPECT_EQ(contents, expected_contents);
  if (contents != expected_contents) {
    return false;
  }
  return true;
}

TEST_P(CommonProtocolTest, CopyFolder) {
  CommonProtocolTestInit init;
  init.folders = {"/folder-1",      "/folder-2",       "/folder-3",
                  "/folder-4",      "/folder-3/sub-1", "/folder-3/sub-2",
                  "/folder-4/sub-2"};
  init.files = {{"/file-0", "root file!"},
                {"/folder-2/file-1", "1234567890"},
                {"/folder-2/file-2", "abcdefghij"},
                {"/folder-4/file-1", "0987654321"},
                {"/folder-4/sub-2/file-2", "ABCDEFGHIJ"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kFolderCreate)) {
    return;
  }
  const bool check_info =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kInfo);
  const bool check_contents =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileRead);

  EXPECT_FALSE(file_system.CopyFolder("test:/folder-1", "test:/file-0"));
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/file-0"));

  EXPECT_FALSE(file_system.CopyFolder("test:/file-0", "test:/new-folder"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/new-folder"));

  EXPECT_FALSE(file_system.CopyFolder("test:/invalid", "test:/new-folder"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/new-folder"));

  EXPECT_TRUE(file_system.CopyFolder("test:/folder-1", "test:/new-folder-1"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/new-folder-1"));

  EXPECT_TRUE(file_system.CopyFolder("test:/folder-1", "test:/new-folder-1"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/new-folder-1"));

  EXPECT_TRUE(file_system.CopyFolder("test:/folder-2", "test:/new-folder-2"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/new-folder-2"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFile("test:/new-folder-2/file-1"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/new-folder-2/file-1", "1234567890"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFile("test:/new-folder-2/file-2"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/new-folder-2/file-2", "abcdefghij"));

  EXPECT_TRUE(file_system.CopyFolder("test:/folder-2", "test:/new-folder-2"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/new-folder-2"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFile("test:/new-folder-2/file-1"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/new-folder-2/file-1", "1234567890"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFile("test:/new-folder-2/file-2"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/new-folder-2/file-2", "abcdefghij"));

  EXPECT_TRUE(file_system.CopyFolder("test:/folder-2",
                                     "test:/new-folder-1/new-folder-3"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFolder("test:/new-folder-1/new-folder-3"));
  EXPECT_TRUE(!check_info || file_system.IsValidFile(
                                 "test:/new-folder-1/new-folder-3/file-1"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system,
                            "test:/new-folder-1/new-folder-3/file-1",
                            "1234567890"));
  EXPECT_TRUE(!check_info || file_system.IsValidFile(
                                 "test:/new-folder-1/new-folder-3/file-2"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system,
                            "test:/new-folder-1/new-folder-3/file-2",
                            "abcdefghij"));

  EXPECT_TRUE(file_system.CopyFolder("test:/folder-3", "test:/new-folder-2"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/new-folder-2"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFolder("test:/new-folder-2/sub-1"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFolder("test:/new-folder-2/sub-2"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFile("test:/new-folder-2/file-1"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/new-folder-2/file-1", "1234567890"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFile("test:/new-folder-2/file-2"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/new-folder-2/file-2", "abcdefghij"));

  EXPECT_TRUE(file_system.CopyFolder("test:/folder-4", "test:/folder-2"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/folder-2"));
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/folder-2/file-1"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/folder-2/file-1", "0987654321"));
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/folder-2/file-2"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/folder-2/file-2", "abcdefghij"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/folder-2/sub-2"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFile("test:/folder-2/sub-2/file-2"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/folder-2/sub-2/file-2", "ABCDEFGHIJ"));

  EXPECT_TRUE(file_system.CopyFolder("test:/folder-4", "test:/folder-3"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/folder-3/sub-1"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/folder-3/sub-2"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFile("test:/folder-3/sub-2/file-2"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/folder-3/sub-2/file-2", "ABCDEFGHIJ"));
}

TEST_P(CommonProtocolTest, CopyFile) {
  CommonProtocolTestInit init;
  init.folders = {"/folder-1"};
  init.files = {{"/file-1", "1234567890"}, {"/file-2", "abcde"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileCreate)) {
    return;
  }
  const bool check_info =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kInfo);
  const bool check_contents =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileRead);

  EXPECT_FALSE(file_system.CopyFile("test:/invalid", "test:/new-file"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/new-file"));

  EXPECT_FALSE(file_system.CopyFile("test:/folder-1", "test:/new-file"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/new-file"));

  EXPECT_TRUE(file_system.CopyFile("test:/file-1", "test:/new-file"));
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/new-file"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/new-file", "1234567890"));

  EXPECT_TRUE(file_system.CopyFile("test:/file-2", "test:/new-file"));
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/new-file"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/new-file", "abcde"));

  EXPECT_TRUE(file_system.CopyFile("test:/file-1", "test:/folder-1/new-file"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFile("test:/folder-1/new-file"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/folder-1/new-file", "1234567890"));

  EXPECT_TRUE(
      file_system.CopyFile("test:/folder-1/new-file", "test:/new-file"));
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/new-file"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/new-file", "1234567890"));
}

TEST_P(CommonProtocolTest, CreateFile) {
  CommonProtocolTestInit init;
  init.folders = {"/folder-1"};
  init.files = {{"/file-1", "1234567890"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileCreate)) {
    return;
  }
  const bool check_info =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kInfo);
  const bool check_contents =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileRead);
  std::unique_ptr<File> file;

  file = file_system.OpenFile("test:/folder-1", kNewFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/folder-1"));

  file = file_system.OpenFile("test:/file-1", kNewFileFlags - FileFlag::kReset);
  EXPECT_NE(file, nullptr);
  file.reset();
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/file-1"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file-1", "1234567890"));

  file = file_system.OpenFile("test:/file-1", kNewFileFlags);
  EXPECT_NE(file, nullptr);
  file.reset();
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/file-1"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file-1", ""));

  file = file_system.OpenFile("test:/folder-1/file-2",
                              kNewFileFlags - FileFlag::kReset);
  EXPECT_NE(file, nullptr);
  file.reset();
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/folder-1/file-2"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/folder-1/file-2", ""));

  file = file_system.OpenFile("test:/file-3", kNewFileFlags);
  EXPECT_NE(file, nullptr);
  file.reset();
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/file-3"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file-3", ""));
}

TEST_P(CommonProtocolTest, ReadFile) {
  CommonProtocolTestInit init;
  static constexpr int64_t kFileSize = 100000;
  init.files = {{"/file", GenerateTestString(kFileSize)}};
  const std::string& file_contents = std::get<1>(init.files[0]);
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileRead)) {
    return;
  }
  std::unique_ptr<File> file =
      file_system.OpenFile("test:/file", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  std::string contents;

  contents = file->ReadString(100);
  EXPECT_EQ(contents, file_contents.substr(0, 100));
  EXPECT_EQ(file->GetPosition(), 100);

  contents = file->ReadString(200);
  EXPECT_EQ(contents, file_contents.substr(100, 200));
  EXPECT_EQ(file->GetPosition(), 300);

  EXPECT_EQ(file->SeekEnd(), kFileSize);
  EXPECT_EQ(file->SeekBy(-50), kFileSize - 50);
  contents = file->ReadRemainingString();
  EXPECT_EQ(contents, file_contents.substr(kFileSize - 50, 50));
  EXPECT_EQ(file->GetPosition(), kFileSize);

  EXPECT_EQ(file->SeekTo(kFileSize / 4), kFileSize / 4);
  contents = file->ReadString(kFileSize / 2);
  EXPECT_EQ(contents, file_contents.substr(kFileSize / 4, kFileSize / 2));
  EXPECT_EQ(file->GetPosition(), kFileSize / 4 + kFileSize / 2);
}

TEST_P(CommonProtocolTest, ReadFilePastEnd) {
  CommonProtocolTestInit init;
  static constexpr int64_t kFileSize = 100;
  init.files = {{"/file", GenerateTestString(kFileSize)}};
  const std::string& file_contents = std::get<1>(init.files[0]);
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileRead)) {
    return;
  }
  std::unique_ptr<File> file =
      file_system.OpenFile("test:/file", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  std::string contents;

  contents = file->ReadString(kFileSize);
  EXPECT_EQ(contents, file_contents);
  EXPECT_EQ(file->GetPosition(), kFileSize);

  contents = file->ReadString(1);
  EXPECT_THAT(contents, IsEmpty());
  EXPECT_EQ(file->GetPosition(), kFileSize);

  file->SeekTo(kFileSize / 2);
  contents = file->ReadString(kFileSize);
  EXPECT_EQ(contents, file_contents.substr(kFileSize / 2));
  EXPECT_EQ(file->GetPosition(), kFileSize);
}

TEST_P(CommonProtocolTest, WriteFile) {
  CommonProtocolTestInit init;
  init.files = {{"/file", "1234567890"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileWrite)) {
    return;
  }
  const bool check_contents =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileRead);
  std::unique_ptr<File> file;
  static constexpr int64_t kFileSize = 100000;
  const std::string contents = GenerateTestString(kFileSize);

  file = file_system.OpenFile("test:/file", kWriteFileFlags);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->WriteString("abcde"), 5);
  EXPECT_EQ(file->GetPosition(), 5);
  file.reset();
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file", "abcde67890"));

  file = file_system.OpenFile("test:/file", kWriteFileFlags + FileFlag::kReset);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->WriteString("abcde"), 5);
  EXPECT_EQ(file->GetPosition(), 5);
  file.reset();
  EXPECT_TRUE(!check_contents || CheckContents(file_system, "test:/file", "abcde"));

  file = file_system.OpenFile("test:/file", kWriteFileFlags);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->WriteString(contents.substr(0, 100)), 100);
  EXPECT_EQ(file->GetPosition(), 100);
  EXPECT_EQ(file->WriteString(contents.substr(100, 200)), 200);
  EXPECT_EQ(file->GetPosition(), 300);
  EXPECT_EQ(file->SeekTo(50), 50);
  EXPECT_EQ(file->WriteString(contents.substr(kFileSize / 4, kFileSize / 2)),
            kFileSize / 2);
  EXPECT_EQ(file->GetPosition(), 50 + kFileSize / 2);
  file.reset();
  std::string expected_contents = absl::StrCat(
      contents.substr(0, 50), contents.substr(kFileSize / 4, kFileSize / 2));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file", expected_contents));
}

}  // namespace
}  // namespace gb
