// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/file_protocol.h"

#include "gb/file/raw_file.h"
#include "gb/file/test_protocol.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(FileProtocolTest, List) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/empty"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/subfolder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/file-1"] = TestProtocol::PathState::NewFile("1");
  state.paths["/folder/file-2"] = TestProtocol::PathState::NewFile("12");
  state.paths["/folder/invalid"] = {};
  state.paths["/folder/subfolder/file-1"] =
      TestProtocol::PathState::NewFile("A");
  state.paths["/folder/subfolder/file-2"] =
      TestProtocol::PathState::NewFile("AB");
  TestProtocol protocol(&state);
  std::vector<std::string> paths;

  paths =
      protocol.List("test", "/file", "", FolderMode::kNormal, kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 0);
  state.ResetCounts();

  paths =
      protocol.List("test", "/invalid", "", FolderMode::kNormal, kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 0);
  state.ResetCounts();

  paths = protocol.List("test", "/", "", FolderMode::kNormal, kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>({"test:/file", "test:/folder"}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.List("test", "/", "", FolderMode::kNormal, PathType::kFile);
  EXPECT_EQ(paths, std::vector<std::string>({"test:/file"}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths =
      protocol.List("test", "/", "", FolderMode::kNormal, PathType::kFolder);
  EXPECT_EQ(paths, std::vector<std::string>({"test:/folder"}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.List("test", "/", "fil", FolderMode::kNormal, kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths =
      protocol.List("test", "/", "file", FolderMode::kNormal, kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>({"test:/file"}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths =
      protocol.List("test", "/", "f*e*", FolderMode::kNormal, kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>({"test:/file", "test:/folder"}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.List("test", "/folder", "file-*", FolderMode::kNormal,
                        kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>(
                       {"test:/folder/file-1", "test:/folder/file-2"}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.List("test", "/folder", "file-*", FolderMode::kRecursive,
                        kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>({
                       "test:/folder/file-1",
                       "test:/folder/file-2",
                       "test:/folder/subfolder/file-1",
                       "test:/folder/subfolder/file-2",
                   }));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 3);
  state.ResetCounts();

  paths = protocol.List("test", "/folder", "", FolderMode::kRecursive,
                        kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>({
                       "test:/folder/empty",
                       "test:/folder/file-1",
                       "test:/folder/file-2",
                       "test:/folder/subfolder",
                       "test:/folder/subfolder/file-1",
                       "test:/folder/subfolder/file-2",
                   }));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 3);
  state.ResetCounts();

  paths = protocol.List("test", "/folder", "", FolderMode::kRecursive,
                        PathType::kFile);
  EXPECT_EQ(paths, std::vector<std::string>({
                       "test:/folder/file-1",
                       "test:/folder/file-2",
                       "test:/folder/subfolder/file-1",
                       "test:/folder/subfolder/file-2",
                   }));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 3);
  state.ResetCounts();

  paths = protocol.List("test", "/folder", "", FolderMode::kRecursive,
                        PathType::kFolder);
  EXPECT_EQ(paths, std::vector<std::string>({
                       "test:/folder/empty",
                       "test:/folder/subfolder",
                   }));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 3);
  state.ResetCounts();
}

TEST(FileProtocolTest, CreateFolder) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_TRUE(protocol.CreateFolder("test", "/", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 0);
  state.ResetCounts();

  EXPECT_TRUE(protocol.CreateFolder("test", "/folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CreateFolder("test", "/file", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 0);
  state.ResetCounts();

  EXPECT_TRUE(
      protocol.CreateFolder("test", "/new-folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  EXPECT_EQ(state.paths["/new-folder"].GetType(), PathType::kFolder);
  state.ResetCounts();

  state.fail_path = "/folder/new-folder";
  EXPECT_FALSE(
      protocol.CreateFolder("test", "/folder/new-folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  EXPECT_EQ(state.paths["/folder/new-folder"].GetType(), PathType::kInvalid);
  state.ResetCounts();
  state.fail_path.clear();

  EXPECT_TRUE(
      protocol.CreateFolder("test", "/folder/new-folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  EXPECT_EQ(state.paths["/folder/new-folder"].GetType(), PathType::kFolder);
  state.ResetCounts();

  EXPECT_FALSE(
      protocol.CreateFolder("test", "/file/new-folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(
      protocol.CreateFolder("test", "/folder/a/b/c", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 0);
  state.ResetCounts();

  state.fail_path = "/folder/a/b";
  EXPECT_FALSE(
      protocol.CreateFolder("test", "/folder/a/b/c", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 2);
  EXPECT_EQ(state.paths["/folder/a"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/folder/a/b"].GetType(), PathType::kInvalid);
  EXPECT_EQ(state.paths["/folder/a/b/c"].GetType(), PathType::kInvalid);
  state.ResetCounts();
  state.fail_path.clear();

  EXPECT_TRUE(
      protocol.CreateFolder("test", "/folder/a/b/c", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 2);
  EXPECT_EQ(state.paths["/folder/a"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/folder/a/b"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/folder/a/b/c"].GetType(), PathType::kFolder);
  state.ResetCounts();

  EXPECT_FALSE(
      protocol.CreateFolder("test", "/file/a/b/c", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 0);
  state.ResetCounts();
}

TEST(FileProtocolTest, CopyFolder) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/empty"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/empty"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/file-1"] = TestProtocol::PathState::NewFile("abcdef");
  state.paths["/folder/file-2"] = TestProtocol::PathState::NewFile("uvwxyz");
  state.paths["/folder/sub-1"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/sub-1/file-1"] =
      TestProtocol::PathState::NewFile("ABCDEF");
  state.paths["/folder/sub-1/file-2"] =
      TestProtocol::PathState::NewFile("UVWXYZ");
  state.paths["/folder/sub-2"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/sub-2/file-1"] =
      TestProtocol::PathState::NewFile("hijklm");
  state.paths["/folder/sub-2/file-3"] =
      TestProtocol::PathState::NewFile("nopqrs");
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.CopyFolder("test", "/file", "test", "/folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFolder("test", "/invalid", "test", "/folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFolder("test", "/folder", "test", "/file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();

  state.fail_path = "/new-folder";
  EXPECT_FALSE(protocol.CopyFolder("test", "/folder", "test", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();
  state.fail_path.clear();

  EXPECT_TRUE(protocol.CopyFolder("test", "/folder", "test", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 4);
  EXPECT_EQ(state.copy_file_count, 6);
  EXPECT_EQ(state.paths["/new-folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder/empty"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder/file-1"].GetContents(), "abcdef");
  EXPECT_EQ(state.paths["/new-folder/file-2"].GetContents(), "uvwxyz");
  EXPECT_EQ(state.paths["/new-folder/sub-1"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder/sub-1/file-1"].GetContents(), "ABCDEF");
  EXPECT_EQ(state.paths["/new-folder/sub-1/file-2"].GetContents(), "UVWXYZ");
  EXPECT_EQ(state.paths["/new-folder/sub-2"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder/sub-2/file-1"].GetContents(), "hijklm");
  EXPECT_EQ(state.paths["/new-folder/sub-2/file-3"].GetContents(), "nopqrs");
  state.ResetCounts();

  EXPECT_TRUE(protocol.CopyFolder("test", "/folder", "test", "/empty"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 4);
  EXPECT_EQ(state.copy_file_count, 6);
  EXPECT_EQ(state.paths["/empty"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/empty/empty"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/empty/file-1"].GetContents(), "abcdef");
  EXPECT_EQ(state.paths["/empty/file-2"].GetContents(), "uvwxyz");
  EXPECT_EQ(state.paths["/empty/sub-1"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/empty/sub-1/file-1"].GetContents(), "ABCDEF");
  EXPECT_EQ(state.paths["/empty/sub-1/file-2"].GetContents(), "UVWXYZ");
  EXPECT_EQ(state.paths["/empty/sub-2"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/empty/sub-2/file-1"].GetContents(), "hijklm");
  EXPECT_EQ(state.paths["/empty/sub-2/file-3"].GetContents(), "nopqrs");
  state.ResetCounts();

  EXPECT_TRUE(
      protocol.CopyFolder("test", "/folder/sub-1", "test", "/folder/sub-2"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 2);
  EXPECT_EQ(state.paths["/folder/sub-2/file-1"].GetContents(), "ABCDEF");
  EXPECT_EQ(state.paths["/folder/sub-2/file-2"].GetContents(), "UVWXYZ");
  EXPECT_EQ(state.paths["/folder/sub-2/file-3"].GetContents(), "nopqrs");
  state.ResetCounts();

  EXPECT_TRUE(protocol.CopyFolder("test", "/folder", "test", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 4);
  EXPECT_EQ(state.copy_file_count, 7);
  EXPECT_EQ(state.paths["/new-folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder/empty"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder/file-1"].GetContents(), "abcdef");
  EXPECT_EQ(state.paths["/new-folder/file-2"].GetContents(), "uvwxyz");
  EXPECT_EQ(state.paths["/new-folder/sub-1"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder/sub-1/file-1"].GetContents(), "ABCDEF");
  EXPECT_EQ(state.paths["/new-folder/sub-1/file-2"].GetContents(), "UVWXYZ");
  EXPECT_EQ(state.paths["/new-folder/sub-2"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder/sub-2/file-1"].GetContents(), "ABCDEF");
  EXPECT_EQ(state.paths["/new-folder/sub-2/file-2"].GetContents(), "UVWXYZ");
  EXPECT_EQ(state.paths["/new-folder/sub-2/file-3"].GetContents(), "nopqrs");
  state.ResetCounts();

  state.paths["/folder/sub-1/file-1"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/sub-1/file-2"] = {};
  EXPECT_FALSE(
      protocol.CopyFolder("test", "/folder/sub-1", "test", "/folder/sub-2"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 2);
  EXPECT_EQ(state.copy_file_count, 0);
  EXPECT_EQ(state.paths["/folder/sub-2/file-1"].GetType(), PathType::kFile);
  state.ResetCounts();

  EXPECT_FALSE(
      protocol.CopyFolder("test", "/folder/sub-2", "test", "/folder/sub-1"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.paths["/folder/sub-1/file-1"].GetType(), PathType::kFolder);
  state.ResetCounts();
}

TEST(FileProtocolTest, DeleteFolder) {
  TestProtocol::State state;
  state.name = "test";
  TestProtocol protocol(&state);

  EXPECT_TRUE(protocol.DeleteFolder("test", "/invalid", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  state.ResetCounts();

  EXPECT_TRUE(
      protocol.DeleteFolder("test", "/invalid", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.DeleteFolder("test", "/", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.DeleteFolder("test", "/", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  state.ResetCounts();

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  EXPECT_FALSE(protocol.DeleteFolder("test", "/file", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  EXPECT_EQ(state.paths["/file"].GetType(), PathType::kFile);
  state.ResetCounts();

  EXPECT_FALSE(protocol.DeleteFolder("test", "/file", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  EXPECT_EQ(state.paths["/file"].GetType(), PathType::kFile);
  state.ResetCounts();

  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  EXPECT_TRUE(protocol.DeleteFolder("test", "/folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kInvalid);
  state.ResetCounts();

  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  EXPECT_TRUE(protocol.DeleteFolder("test", "/folder", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kInvalid);
  state.ResetCounts();

  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/sub-1"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/sub-2"] = TestProtocol::PathState::NewFolder();
  EXPECT_FALSE(protocol.DeleteFolder("test", "/folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/folder/sub-1"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/folder/sub-2"].GetType(), PathType::kFolder);
  state.ResetCounts();

  state.fail_path = "/folder/sub-1";
  EXPECT_FALSE(
      protocol.DeleteFolder("test", "/folder", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 2);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/folder/sub-1"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/folder/sub-2"].GetType(), PathType::kFolder);
  state.ResetCounts();
  state.fail_path.clear();

  EXPECT_TRUE(protocol.DeleteFolder("test", "/folder", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 3);
  EXPECT_EQ(state.basic_delete_folder_count, 3);
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kInvalid);
  EXPECT_EQ(state.paths["/folder/sub-1"].GetType(), PathType::kInvalid);
  EXPECT_EQ(state.paths["/folder/sub-2"].GetType(), PathType::kInvalid);
  state.ResetCounts();

  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/file-1"] = TestProtocol::PathState::NewFile("12345");
  state.paths["/folder/file-2"] = TestProtocol::PathState::NewFile("67890");
  EXPECT_FALSE(protocol.DeleteFolder("test", "/folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  EXPECT_EQ(state.delete_file_count, 0);
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/folder/file-1"].GetType(), PathType::kFile);
  EXPECT_EQ(state.paths["/folder/file-2"].GetType(), PathType::kFile);
  state.ResetCounts();

  state.fail_path = "/folder/file-1";
  EXPECT_FALSE(
      protocol.DeleteFolder("test", "/folder", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  EXPECT_EQ(state.delete_file_count, 1);
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/folder/file-1"].GetType(), PathType::kFile);
  EXPECT_EQ(state.paths["/folder/file-2"].GetType(), PathType::kFile);
  state.ResetCounts();
  state.fail_path.clear();

  EXPECT_TRUE(protocol.DeleteFolder("test", "/folder", FolderMode::kRecursive));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  EXPECT_EQ(state.delete_file_count, 2);
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kInvalid);
  EXPECT_EQ(state.paths["/folder/file-1"].GetType(), PathType::kInvalid);
  EXPECT_EQ(state.paths["/folder/file-2"].GetType(), PathType::kInvalid);
  state.ResetCounts();
}

TEST(FileProtocolTest, CopyFile) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("12345");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.CopyFile("test", "/invalid", "test", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/invalid", "test", "/file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/folder", "test", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/folder", "test", "/file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/file", "test", "/folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/file", "test", "/folder/sub/file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();
}

TEST(FileProtocolTest, DeleteFile) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.DeleteFile("test", "/folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_file_count, 1);
  EXPECT_EQ(state.basic_delete_file_count, 0);
  state.ResetCounts();

  EXPECT_TRUE(protocol.DeleteFile("test", "/invalid"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.delete_file_count, 1);
  EXPECT_EQ(state.basic_delete_file_count, 0);
  state.ResetCounts();
}

TEST(FileProtocolTest, OpenFile) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("12345");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_EQ(protocol.OpenFile("test", "/folder", kReadFileFlags), nullptr);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 0);
  state.ResetCounts();

  EXPECT_EQ(protocol.OpenFile("test", "/invalid", kReadFileFlags), nullptr);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 0);
  state.ResetCounts();

  EXPECT_NE(protocol.OpenFile("test", "/folder/new-file", kNewFileFlags),
            nullptr);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  EXPECT_EQ(state.paths["/folder/new-file"].GetType(), PathType::kFile);
  state.ResetCounts();
}

TEST(FileProtocolTest, BasicCopyFile) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("12345");
  TestProtocol protocol(&state);

  state.open_fail_path = "/file";
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "test", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(state.paths["/new-file"].GetType(), PathType::kInvalid);
  state.ResetCounts();
  state.open_fail_path.clear();

  state.open_fail_path = "/new-file";
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "test", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.open_file_count, 2);
  EXPECT_EQ(state.basic_open_file_count, 2);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(state.paths["/new-file"].GetType(), PathType::kInvalid);
  state.ResetCounts();
  state.open_fail_path.clear();

  state.paths["/new-file"] = TestProtocol::PathState::NewFile("67890");
  state.paths["/new-file"].GetFile()->position = -1;
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "test", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.open_file_count, 2);
  EXPECT_EQ(state.basic_open_file_count, 2);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(state.paths["/new-file"].GetContents(), "");
  state.ResetCounts();
  state.fail_path.clear();

  auto* new_file_state = state.paths["/new-file"].GetFile();
  new_file_state->fail_flush = true;
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "test", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(new_file_state->flush_count, 1);
  state.ResetCounts();
  new_file_state->fail_flush = false;

  auto* file_state = state.paths["/file"].GetFile();
  file_state->fail_read_after = 3;
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "test", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(file_state->read_count, 1);
  state.ResetCounts();
  file_state->fail_read_after = -1;

  EXPECT_TRUE(protocol.BasicCopyFile("test", "/file", "test", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(new_file_state->flush_count, 1);
  EXPECT_EQ(state.paths["/new-file"].GetContents(), "12345");
}

TEST(FileProtocolTest, GetPathInfoReportsAttributes) {
  TestProtocol::State state;
  state.name = "test";
  state.attribute_flags = kAllFileAttributes;
  state.paths["/file"] = TestProtocol::PathState::NewFile("12345");
  state.paths["/file"].SetAttributes(FileAttribute::kReadOnly);
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder"].SetAttributes(FileAttribute::kHidden);
  TestProtocol protocol(&state);

  EXPECT_EQ(protocol.GetPathInfo("test", "/file").attributes,
            FileAttribute::kReadOnly);
  EXPECT_EQ(protocol.GetPathInfo("test", "/folder").attributes,
            FileAttribute::kHidden);
  EXPECT_TRUE(protocol.GetPathInfo("test", "/").attributes.IsEmpty());
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileProtocolTest, SetAttributes) {
  TestProtocol::State state;
  state.name = "test";
  state.attribute_flags = kAllFileAttributes;
  state.paths["/file"] = TestProtocol::PathState::NewFile("12345");
  state.paths["/file"].SetAttributes(FileAttribute::kHidden);
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.SetAttributes("test", "file", {}, kAllFileAttributes));
  EXPECT_EQ(state.set_attributes_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(
      protocol.SetAttributes("test", "/invalid", {}, kAllFileAttributes));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.set_attributes_count, 1);
  EXPECT_EQ(state.basic_set_attributes_count, 0);
  state.ResetCounts();

  // Nothing changes, so nothing is set.
  EXPECT_TRUE(protocol.SetAttributes("test", "/file", FileAttribute::kHidden,
                                     FileAttribute::kHidden));
  EXPECT_TRUE(protocol.SetAttributes("test", "/file", {}, {}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.set_attributes_count, 2);
  EXPECT_EQ(state.basic_set_attributes_count, 0);
  state.ResetCounts();

  // Attributes outside the mask are kept.
  EXPECT_TRUE(protocol.SetAttributes("test", "/file", FileAttribute::kReadOnly,
                                     FileAttribute::kReadOnly));
  EXPECT_EQ(state.paths["/file"].GetAttributes(), kAllFileAttributes);
  EXPECT_TRUE(
      protocol.SetAttributes("test", "/file", {}, FileAttribute::kHidden));
  EXPECT_EQ(state.paths["/file"].GetAttributes(), FileAttribute::kReadOnly);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_set_attributes_count, 2);
  state.ResetCounts();

  // The whole mask replaces every attribute.
  EXPECT_TRUE(protocol.SetAttributes("test", "/file", FileAttribute::kHidden,
                                     kAllFileAttributes));
  EXPECT_EQ(state.paths["/file"].GetAttributes(), FileAttribute::kHidden);
  EXPECT_TRUE(protocol.SetAttributes("test", "/folder", kAllFileAttributes,
                                     kAllFileAttributes));
  EXPECT_EQ(state.paths["/folder"].GetAttributes(), kAllFileAttributes);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_set_attributes_count, 2);
  EXPECT_EQ(state.lock_type, FileProtocol::LockType::kInvalid);
  state.ResetCounts();

  // The root has no attributes.
  EXPECT_TRUE(protocol.SetAttributes("test", "/", {}, kAllFileAttributes));
  EXPECT_FALSE(protocol.SetAttributes("test", "/", FileAttribute::kHidden,
                                      kAllFileAttributes));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_set_attributes_count, 0);
  state.ResetCounts();

  state.fail_path = "/file";
  EXPECT_FALSE(protocol.SetAttributes("test", "/file", {}, kAllFileAttributes));
  EXPECT_EQ(state.paths["/file"].GetAttributes(), FileAttribute::kHidden);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_set_attributes_count, 1);
}

TEST(FileProtocolTest, CopyFileKeepsAttributes) {
  TestProtocol::State state;
  state.name = "test";
  state.attribute_flags = kAllFileAttributes;
  state.paths["/file"] = TestProtocol::PathState::NewFile("12345");
  state.paths["/file"].SetAttributes(FileAttribute::kReadOnly);
  state.paths["/plain"] = TestProtocol::PathState::NewFile("abc");
  state.paths["/hidden"] = TestProtocol::PathState::NewFile("67890");
  state.paths["/hidden"].SetAttributes(FileAttribute::kHidden);
  TestProtocol protocol(&state);

  EXPECT_TRUE(protocol.CopyFile("test", "/file", "test", "/new-file"));
  EXPECT_EQ(state.paths["/new-file"].GetContents(), "12345");
  EXPECT_EQ(state.paths["/new-file"].GetAttributes(), FileAttribute::kReadOnly);
  EXPECT_EQ(state.basic_set_attributes_count, 1);
  state.ResetCounts();

  // A file that is replaced has the source's attributes, not its own.
  EXPECT_TRUE(protocol.CopyFile("test", "/file", "test", "/hidden"));
  EXPECT_EQ(state.paths["/hidden"].GetContents(), "12345");
  EXPECT_EQ(state.paths["/hidden"].GetAttributes(), FileAttribute::kReadOnly);
  state.ResetCounts();

  EXPECT_TRUE(protocol.CopyFile("test", "/plain", "test", "/new-plain"));
  EXPECT_TRUE(state.paths["/new-plain"].GetAttributes().IsEmpty());
  EXPECT_EQ(state.basic_set_attributes_count, 0);
  state.ResetCounts();

  // A protocol's own BasicCopyFile needn't copy attributes.
  state.implement_copy = true;
  EXPECT_TRUE(protocol.CopyFile("test", "/file", "test", "/new-file-2"));
  EXPECT_EQ(state.paths["/new-file-2"].GetAttributes(),
            FileAttribute::kReadOnly);
  state.ResetCounts();

  // Nothing more is done for a protocol without attributes.
  state.attribute_flags = {};
  EXPECT_TRUE(protocol.CopyFile("test", "/plain", "test", "/new-plain-2"));
  EXPECT_EQ(state.set_attributes_count, 0);
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileProtocolTest, CopyFolderKeepsAttributes) {
  TestProtocol::State state;
  state.name = "test";
  state.attribute_flags = kAllFileAttributes;
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder"].SetAttributes(kAllFileAttributes);
  state.paths["/folder/file"] = TestProtocol::PathState::NewFile("12345");
  state.paths["/folder/file"].SetAttributes(FileAttribute::kReadOnly);
  state.paths["/folder/sub"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/sub"].SetAttributes(FileAttribute::kHidden);
  state.paths["/folder/plain"] = TestProtocol::PathState::NewFolder();
  state.paths["/existing"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_TRUE(protocol.CopyFolder("test", "/folder", "test", "/new-folder"));
  EXPECT_EQ(state.paths["/new-folder"].GetAttributes(), kAllFileAttributes);
  EXPECT_EQ(state.paths["/new-folder/file"].GetAttributes(),
            FileAttribute::kReadOnly);
  EXPECT_EQ(state.paths["/new-folder/sub"].GetAttributes(),
            FileAttribute::kHidden);
  EXPECT_TRUE(state.paths["/new-folder/plain"].GetAttributes().IsEmpty());
  EXPECT_EQ(state.basic_set_attributes_count, 3);
  state.ResetCounts();

  // A folder that already exists keeps its own attributes.
  EXPECT_TRUE(protocol.CopyFolder("test", "/folder", "test", "/existing"));
  EXPECT_TRUE(state.paths["/existing"].GetAttributes().IsEmpty());
  EXPECT_EQ(state.paths["/existing/file"].GetAttributes(),
            FileAttribute::kReadOnly);
  EXPECT_EQ(state.paths["/existing/sub"].GetAttributes(),
            FileAttribute::kHidden);
  EXPECT_EQ(state.invalid_call_count, 0);
}

}  // namespace
}  // namespace gb
