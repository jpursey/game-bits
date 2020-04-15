#include "gbits/file/file_protocol.h"

#include "gbits/file/raw_file.h"
#include "gbits/file/test_protocol.h"
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

  EXPECT_FALSE(protocol.CopyFolder("test", "/file", "/folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFolder("test", "/invalid", "/folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFolder("test", "/folder", "/file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();

  state.fail_path = "/new-folder";
  EXPECT_FALSE(protocol.CopyFolder("test", "/folder", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();
  state.fail_path.clear();

  EXPECT_TRUE(protocol.CopyFolder("test", "/folder", "/new-folder"));
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

  EXPECT_TRUE(protocol.CopyFolder("test", "/folder", "/empty"));
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

  EXPECT_TRUE(protocol.CopyFolder("test", "/folder/sub-1", "/folder/sub-2"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 2);
  EXPECT_EQ(state.paths["/folder/sub-2/file-1"].GetContents(), "ABCDEF");
  EXPECT_EQ(state.paths["/folder/sub-2/file-2"].GetContents(), "UVWXYZ");
  EXPECT_EQ(state.paths["/folder/sub-2/file-3"].GetContents(), "nopqrs");
  state.ResetCounts();

  EXPECT_TRUE(protocol.CopyFolder("test", "/folder", "/new-folder"));
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
  EXPECT_FALSE(protocol.CopyFolder("test", "/folder/sub-1", "/folder/sub-2"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_folder_count, 2);
  EXPECT_EQ(state.copy_file_count, 0);
  EXPECT_EQ(state.paths["/folder/sub-2/file-1"].GetType(), PathType::kFile);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFolder("test", "/folder/sub-2", "/folder/sub-1"));
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

  EXPECT_FALSE(protocol.CopyFile("test", "/invalid", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/invalid", "/file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/folder", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/folder", "/file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/file", "/folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/file", "/folder/sub/file"));
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
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(state.paths["/new-file"].GetType(), PathType::kInvalid);
  state.ResetCounts();
  state.open_fail_path.clear();

  state.open_fail_path = "/new-file";
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.open_file_count, 2);
  EXPECT_EQ(state.basic_open_file_count, 2);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(state.paths["/new-file"].GetType(), PathType::kInvalid);
  state.ResetCounts();
  state.open_fail_path.clear();

  state.paths["/new-file"] = TestProtocol::PathState::NewFile("67890");
  state.paths["/new-file"].GetFile()->position = -1;
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.open_file_count, 2);
  EXPECT_EQ(state.basic_open_file_count, 2);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(state.paths["/new-file"].GetContents(), "");
  state.ResetCounts();
  state.fail_path.clear();
}

}  // namespace
}  // namespace gb
