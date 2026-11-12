// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/common_protocol_test.h"

#include "absl/strings/str_cat.h"
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
    if (file->Write(contents.data(), size) != size || !file->Flush()) {
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
  EXPECT_TRUE(file_system.CreateFolder("test:/folder-1/folder-6",
                                       FolderMode::kRecursive));
  EXPECT_TRUE(file_system.CreateFolder("test:/", FolderMode::kRecursive));
  EXPECT_FALSE(
      file_system.CreateFolder("test:/file-1", FolderMode::kRecursive));
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

  EXPECT_TRUE(file_system.CopyFile("test:/file-2", "test:/file-2"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file-2", "abcde"));
}

TEST_P(CommonProtocolTest, MoveFile) {
  CommonProtocolTestInit init;
  init.folders = {"/folder-1"};
  init.files = {{"/file-1", "1234567890"}, {"/file-2", "abcde"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kMove)) {
    return;
  }
  const bool check_info =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kInfo);
  const bool check_contents =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileRead);

  EXPECT_FALSE(file_system.MoveFile("test:/invalid", "test:/new-file"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/new-file"));

  EXPECT_FALSE(file_system.MoveFile("test:/folder-1", "test:/new-file"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/folder-1"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/new-file"));

  // Nothing at the new path is replaced.
  EXPECT_FALSE(file_system.MoveFile("test:/file-1", "test:/file-2"));
  EXPECT_FALSE(file_system.MoveFile("test:/file-1", "test:/folder-1"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/folder-1"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file-1", "1234567890"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file-2", "abcde"));

  EXPECT_FALSE(file_system.MoveFile("test:/file-1", "test:/invalid/new-file"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/invalid"));

  EXPECT_TRUE(file_system.MoveFile("test:/file-1", "test:/file-1"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file-1", "1234567890"));

  EXPECT_TRUE(file_system.MoveFile("test:/file-1", "test:/folder-1/new-file"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/file-1"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/folder-1/new-file", "1234567890"));

  EXPECT_TRUE(
      file_system.MoveFile("test:/folder-1/new-file", "test:/folder-1/file-1"));
  EXPECT_TRUE(!check_info ||
              !file_system.IsValidPath("test:/folder-1/new-file"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/folder-1/file-1", "1234567890"));
}

TEST_P(CommonProtocolTest, MoveFolder) {
  CommonProtocolTestInit init;
  init.folders = {"/folder-1", "/folder-1/sub-1", "/folder-1-other",
                  "/folder-2"};
  init.files = {{"/file-0", "root file!"},
                {"/folder-1/file-1", "1234567890"},
                {"/folder-1/sub-1/file-2", "abcdefghij"},
                {"/folder-1-other/file-3", "0987654321"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(
          {FileProtocolFlag::kMove, FileProtocolFlag::kFolderCreate})) {
    return;
  }
  const bool check_info =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kInfo);
  const bool check_contents =
      file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileRead);

  EXPECT_FALSE(file_system.MoveFolder("test:/invalid", "test:/new-folder"));
  EXPECT_FALSE(file_system.MoveFolder("test:/file-0", "test:/new-folder"));
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/file-0"));
  EXPECT_FALSE(file_system.MoveFolder("test:/", "test:/new-folder"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/new-folder"));

  // Nothing at the new path is replaced or merged with.
  EXPECT_FALSE(file_system.MoveFolder("test:/folder-1", "test:/folder-2"));
  EXPECT_FALSE(file_system.MoveFolder("test:/folder-1", "test:/file-0"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/folder-1"));
  EXPECT_TRUE(!check_info || file_system.IsValidFile("test:/file-0"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/folder-2/file-1"));

  EXPECT_FALSE(
      file_system.MoveFolder("test:/folder-1", "test:/invalid/new-folder"));
  EXPECT_FALSE(
      file_system.MoveFolder("test:/folder-1", "test:/folder-1/sub-1/new"));
  EXPECT_TRUE(!check_info ||
              !file_system.IsValidPath("test:/folder-1/sub-1/new"));

  EXPECT_TRUE(file_system.MoveFolder("test:/folder-1", "test:/folder-1"));
  EXPECT_TRUE(!check_info || file_system.IsValidFolder("test:/folder-1"));

  EXPECT_TRUE(file_system.MoveFolder("test:/folder-1", "test:/folder-2/moved"));
  EXPECT_TRUE(!check_info || !file_system.IsValidPath("test:/folder-1"));
  EXPECT_TRUE(!check_info ||
              file_system.IsValidFolder("test:/folder-2/moved/sub-1"));
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/folder-2/moved/file-1", "1234567890"));
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/folder-2/moved/sub-1/file-2",
                            "abcdefghij"));

  // A folder whose name starts with the moved folder's stays where it is.
  EXPECT_TRUE(
      !check_contents ||
      CheckContents(file_system, "test:/folder-1-other/file-3", "0987654321"));
  if (file_system.GetFlags("test").IsSet(FileProtocolFlag::kList)) {
    EXPECT_THAT(file_system.List("test:/", FolderMode::kRecursive),
                UnorderedElementsAre("test:/file-0", "test:/folder-1-other",
                                     "test:/folder-1-other/file-3",
                                     "test:/folder-2", "test:/folder-2/moved",
                                     "test:/folder-2/moved/file-1",
                                     "test:/folder-2/moved/sub-1",
                                     "test:/folder-2/moved/sub-1/file-2"));
  }
}

// Every FileAttribute, to test each one a protocol supports.
constexpr FileAttribute kEachFileAttribute[] = {FileAttribute::kReadOnly,
                                                FileAttribute::kHidden};

// A new FileAttribute must be added to kEachFileAttribute to be tested.
TEST(CommonProtocolTestSupport, EachFileAttribute) {
  FileAttributes each_attribute;
  for (FileAttribute attribute : kEachFileAttribute) {
    each_attribute += attribute;
  }
  EXPECT_EQ(each_attribute, kAllFileAttributes);
}

TEST_P(CommonProtocolTest, GetAndSetAttributes) {
  CommonProtocolTestInit init;
  init.folders = {"/folder"};
  init.files = {{"/file", "1234567890"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  const FileAttributes attribute_flags = file_system.GetAttributeFlags("test");
  if (attribute_flags.IsEmpty()) {
    return;
  }

  // Nothing has attributes until they are set.
  EXPECT_TRUE(file_system.GetPathInfo("test:/").attributes.IsEmpty());
  EXPECT_TRUE(file_system.GetPathInfo("test:/folder").attributes.IsEmpty());
  EXPECT_TRUE(file_system.GetPathInfo("test:/file").attributes.IsEmpty());
  if (!file_system.GetFlags("test").IsSet(FileProtocolFlag::kFileWrite)) {
    EXPECT_FALSE(file_system.SetAttributes("test:/file", attribute_flags));
    return;
  }

  for (std::string_view path : {"test:/file", "test:/folder"}) {
    for (FileAttribute attribute : kEachFileAttribute) {
      if (!attribute_flags.IsSet(attribute)) {
        continue;
      }
      EXPECT_TRUE(file_system.SetAttributes(path, attribute, attribute))
          << path;
      EXPECT_EQ(file_system.GetPathInfo(path).attributes, attribute) << path;

      // A mask leaves the attributes outside it.
      EXPECT_TRUE(file_system.SetAttributes(path, attribute_flags,
                                            attribute_flags - attribute))
          << path;
      EXPECT_EQ(file_system.GetPathInfo(path).attributes, attribute_flags)
          << path;
      EXPECT_TRUE(file_system.SetAttributes(path, {}, attribute)) << path;
      EXPECT_EQ(file_system.GetPathInfo(path).attributes,
                attribute_flags - attribute)
          << path;

      // The default mask replaces every attribute.
      EXPECT_TRUE(file_system.SetAttributes(path, attribute)) << path;
      EXPECT_EQ(file_system.GetPathInfo(path).attributes, attribute) << path;
      EXPECT_TRUE(file_system.SetAttributes(path, {})) << path;
      EXPECT_TRUE(file_system.GetPathInfo(path).attributes.IsEmpty()) << path;
    }
  }

  const FileAttributes unsupported = kAllFileAttributes - attribute_flags;
  if (!unsupported.IsEmpty()) {
    EXPECT_FALSE(file_system.SetAttributes("test:/file", unsupported));
    EXPECT_TRUE(file_system.GetPathInfo("test:/file").attributes.IsEmpty());
  }
  EXPECT_FALSE(file_system.SetAttributes("test:/invalid", attribute_flags));
  EXPECT_FALSE(file_system.IsValidPath("test:/invalid"));

  // The root has no attributes.
  EXPECT_TRUE(file_system.SetAttributes("test:/", {}));
  EXPECT_FALSE(file_system.SetAttributes("test:/", attribute_flags));
  EXPECT_TRUE(file_system.GetPathInfo("test:/").attributes.IsEmpty());
}

TEST_P(CommonProtocolTest, ReadOnlyFile) {
  CommonProtocolTestInit init;
  init.files = {{"/file", "1234567890"}, {"/other", "abcde"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  const FileProtocolFlags flags = file_system.GetFlags("test");
  if (!file_system.GetAttributeFlags("test").IsSet(FileAttribute::kReadOnly) ||
      !flags.IsSet(FileProtocolFlag::kFileWrite)) {
    return;
  }
  const bool check_contents = flags.IsSet(FileProtocolFlag::kFileRead);
  ASSERT_TRUE(file_system.SetAttributes("test:/file", FileAttribute::kReadOnly,
                                        FileAttribute::kReadOnly));

  // It can't be written, or written over.
  EXPECT_EQ(file_system.OpenFile("test:/file", kWriteFileFlags), nullptr);
  EXPECT_EQ(file_system.OpenFile("test:/file", kReadWriteFileFlags), nullptr);
  EXPECT_EQ(
      file_system.OpenFile("test:/file", kWriteFileFlags + FileFlag::kReset),
      nullptr);
  if (flags.IsSet(FileProtocolFlag::kFileCreate)) {
    EXPECT_EQ(file_system.OpenFile("test:/file", kNewFileFlags), nullptr);
    EXPECT_FALSE(file_system.CopyFile("test:/other", "test:/file"));
  }
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file", "1234567890"));

  // It can be moved, keeping its attributes.
  if (flags.IsSet(FileProtocolFlag::kMove)) {
    EXPECT_TRUE(file_system.MoveFile("test:/file", "test:/moved"));
    EXPECT_EQ(file_system.GetPathInfo("test:/moved").attributes,
              FileAttribute::kReadOnly);
    EXPECT_EQ(file_system.OpenFile("test:/moved", kWriteFileFlags), nullptr);
    EXPECT_TRUE(file_system.MoveFile("test:/moved", "test:/file"));
  }

  // Once it isn't read-only, it can be written again.
  EXPECT_TRUE(
      file_system.SetAttributes("test:/file", {}, FileAttribute::kReadOnly));
  auto file = file_system.OpenFile("test:/file", kWriteFileFlags);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->WriteString("abc"), 3);
  file.reset();
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file", "abc4567890"));
}

TEST_P(CommonProtocolTest, CopyFileKeepsAttributes) {
  CommonProtocolTestInit init;
  init.files = {
      {"/file", "1234567890"}, {"/plain", "abcde"}, {"/existing", "ABCDE"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  const FileProtocolFlags flags = file_system.GetFlags("test");
  const FileAttributes attribute_flags = file_system.GetAttributeFlags("test");
  if (attribute_flags.IsEmpty() ||
      !flags.IsSet(FileProtocolFlag::kFileCreate)) {
    return;
  }
  const bool check_contents = flags.IsSet(FileProtocolFlag::kFileRead);
  ASSERT_TRUE(file_system.SetAttributes("test:/file", attribute_flags));

  // A new file, and one written over, get the source's attributes, even when
  // read-only.
  EXPECT_TRUE(file_system.CopyFile("test:/file", "test:/new-file"));
  EXPECT_EQ(file_system.GetPathInfo("test:/new-file").attributes,
            attribute_flags);
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/new-file", "1234567890"));
  EXPECT_TRUE(file_system.CopyFile("test:/file", "test:/existing"));
  EXPECT_EQ(file_system.GetPathInfo("test:/existing").attributes,
            attribute_flags);
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/existing", "1234567890"));

  // A file written over loses attributes the source doesn't have.
  const FileAttributes writable_attributes =
      attribute_flags - FileAttribute::kReadOnly;
  if (!writable_attributes.IsEmpty()) {
    ASSERT_TRUE(
        file_system.SetAttributes("test:/existing", writable_attributes));
    EXPECT_TRUE(file_system.CopyFile("test:/plain", "test:/existing"));
    EXPECT_TRUE(file_system.GetPathInfo("test:/existing").attributes.IsEmpty());
    EXPECT_TRUE(!check_contents ||
                CheckContents(file_system, "test:/existing", "abcde"));
  }
}

TEST_P(CommonProtocolTest, CopyFolderKeepsAttributes) {
  CommonProtocolTestInit init;
  init.folders = {"/folder", "/folder/sub", "/existing"};
  init.files = {{"/folder/file", "1234567890"}, {"/folder/sub/file", "abcde"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  const FileProtocolFlags flags = file_system.GetFlags("test");
  const FileAttributes attribute_flags = file_system.GetAttributeFlags("test");
  if (attribute_flags.IsEmpty() ||
      !flags.IsSet(
          {FileProtocolFlag::kFolderCreate, FileProtocolFlag::kFileCreate})) {
    return;
  }
  const bool check_contents = flags.IsSet(FileProtocolFlag::kFileRead);

  // Read-only folders are copied with everything in them.
  ASSERT_TRUE(file_system.SetAttributes("test:/folder", attribute_flags));
  ASSERT_TRUE(file_system.SetAttributes("test:/folder/sub", attribute_flags));
  ASSERT_TRUE(file_system.SetAttributes("test:/folder/file", attribute_flags));

  EXPECT_TRUE(file_system.CopyFolder("test:/folder", "test:/copy"));
  EXPECT_EQ(file_system.GetPathInfo("test:/copy").attributes, attribute_flags);
  EXPECT_EQ(file_system.GetPathInfo("test:/copy/sub").attributes,
            attribute_flags);
  EXPECT_EQ(file_system.GetPathInfo("test:/copy/file").attributes,
            attribute_flags);
  EXPECT_TRUE(
      file_system.GetPathInfo("test:/copy/sub/file").attributes.IsEmpty());
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/copy/sub/file", "abcde"));

  // A folder that already exists keeps its own attributes.
  EXPECT_TRUE(file_system.CopyFolder("test:/folder", "test:/existing"));
  EXPECT_TRUE(file_system.GetPathInfo("test:/existing").attributes.IsEmpty());
  EXPECT_EQ(file_system.GetPathInfo("test:/existing/sub").attributes,
            attribute_flags);
  EXPECT_EQ(file_system.GetPathInfo("test:/existing/file").attributes,
            attribute_flags);
}

TEST_P(CommonProtocolTest, MoveKeepsAttributes) {
  CommonProtocolTestInit init;
  init.folders = {"/folder"};
  init.files = {{"/file", "1234567890"}, {"/folder/file", "abcde"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  const FileAttributes attribute_flags = file_system.GetAttributeFlags("test");
  if (attribute_flags.IsEmpty() ||
      !file_system.GetFlags("test").IsSet(
          {FileProtocolFlag::kMove, FileProtocolFlag::kFolderCreate})) {
    return;
  }
  ASSERT_TRUE(file_system.SetAttributes("test:/file", attribute_flags));
  ASSERT_TRUE(file_system.SetAttributes("test:/folder", attribute_flags));
  ASSERT_TRUE(file_system.SetAttributes("test:/folder/file", attribute_flags));

  EXPECT_TRUE(file_system.MoveFile("test:/file", "test:/moved-file"));
  EXPECT_EQ(file_system.GetPathInfo("test:/moved-file").attributes,
            attribute_flags);
  EXPECT_TRUE(file_system.MoveFolder("test:/folder", "test:/moved-folder"));
  EXPECT_EQ(file_system.GetPathInfo("test:/moved-folder").attributes,
            attribute_flags);
  EXPECT_EQ(file_system.GetPathInfo("test:/moved-folder/file").attributes,
            attribute_flags);
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

  // A protocol may seek past the end, and reading there reads nothing.
  EXPECT_GE(file->SeekTo(kFileSize * 2), kFileSize);
  contents = file->ReadString(1);
  EXPECT_THAT(contents, IsEmpty());
  EXPECT_TRUE(file->IsValid());
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
  EXPECT_TRUE(!check_contents ||
              CheckContents(file_system, "test:/file", "abcde"));

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

TEST_P(CommonProtocolTest, ReadAndWriteFile) {
  CommonProtocolTestInit init;
  init.files = {{"/file", "1234567890"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(
          {FileProtocolFlag::kFileRead, FileProtocolFlag::kFileWrite})) {
    return;
  }

  // Reads and writes continue from where the last one ended, with no seek
  // in between.
  std::unique_ptr<File> file =
      file_system.OpenFile("test:/file", kReadWriteFileFlags);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->ReadString(2), "12");
  EXPECT_EQ(file->WriteString("ab"), 2);
  EXPECT_EQ(file->GetPosition(), 4);
  EXPECT_EQ(file->ReadString(2), "56");
  EXPECT_EQ(file->WriteString("cd"), 2);
  EXPECT_EQ(file->ReadRemainingString(), "90");
  EXPECT_EQ(file->WriteString("ef"), 2);
  EXPECT_EQ(file->GetPosition(), 12);
  file.reset();
  EXPECT_TRUE(CheckContents(file_system, "test:/file", "12ab56cd90ef"));
}

TEST_P(CommonProtocolTest, FlushAndCloseFile) {
  CommonProtocolTestInit init;
  init.files = {{"/file", "1234567890"}};
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(NewProtocol(init), "test"));
  if (!file_system.GetFlags("test").IsSet(
          {FileProtocolFlag::kFileRead, FileProtocolFlag::kFileWrite})) {
    return;
  }

  // Flushing after a read or a write keeps the position, and what was written.
  std::unique_ptr<File> file =
      file_system.OpenFile("test:/file", kReadWriteFileFlags);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->ReadString(2), "12");
  EXPECT_TRUE(file->Flush());
  EXPECT_EQ(file->ReadString(2), "34");
  EXPECT_EQ(file->WriteString("ab"), 2);
  EXPECT_TRUE(file->Flush());
  EXPECT_EQ(file->GetPosition(), 6);
  EXPECT_EQ(file->ReadString(2), "78");
  EXPECT_EQ(file->WriteString("cd"), 2);
  EXPECT_TRUE(file->Close());
  EXPECT_FALSE(file->IsValid());
  file.reset();
  EXPECT_TRUE(CheckContents(file_system, "test:/file", "1234ab78cd"));
}

}  // namespace
}  // namespace gb
