#include "gbits/file/test_protocol.h"

#include <cstring>

#include "gbits/file/common_protocol_test.h"
#include "gbits/file/raw_file.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Values;

std::unique_ptr<FileProtocol> TestProtocolFactory(
    FileProtocolFlags flags, const CommonProtocolTestInit& init) {
  TestProtocol::State* state = new TestProtocol::State;
  state->flags = flags;
  state->implement_copy = true;
  state->delete_state = true;
  for (const auto& path : init.folders) {
    state->paths[path] = TestProtocol::PathState::NewFolder();
  }
  for (const auto& [path, contents] : init.files) {
    state->paths[path] = TestProtocol::PathState::NewFile(contents);
  }
  return std::make_unique<TestProtocol>(state);
}

std::unique_ptr<FileProtocol> TestProtocolFactory_All(
    const CommonProtocolTestInit& init) {
  return TestProtocolFactory(kAllFileProtocolFlags, init);
}

std::unique_ptr<FileProtocol> TestProtocolFactory_Info(
    const CommonProtocolTestInit& init) {
  return TestProtocolFactory(
      {FileProtocolFlag::kInfo, FileProtocolFlag::kFileRead}, init);
}

std::unique_ptr<FileProtocol> TestProtocolFactory_List(
    const CommonProtocolTestInit& init) {
  return TestProtocolFactory({FileProtocolFlag::kInfo, FileProtocolFlag::kList,
                              FileProtocolFlag::kFileRead},
                             init);
}

std::unique_ptr<FileProtocol> TestProtocolFactory_FolderCreate(
    const CommonProtocolTestInit& init) {
  return TestProtocolFactory(
      {FileProtocolFlag::kInfo, FileProtocolFlag::kList,
       FileProtocolFlag::kFolderCreate, FileProtocolFlag::kFileCreate,
       FileProtocolFlag::kFileWrite},
      init);
}

std::unique_ptr<FileProtocol> TestProtocolFactory_FileCreate(
    const CommonProtocolTestInit& init) {
  return TestProtocolFactory(
      {FileProtocolFlag::kInfo, FileProtocolFlag::kFileCreate,
       FileProtocolFlag::kFileWrite},
      init);
}

std::unique_ptr<FileProtocol> TestProtocolFactory_FileRead(
    const CommonProtocolTestInit& init) {
  return TestProtocolFactory(
      {FileProtocolFlag::kInfo, FileProtocolFlag::kFileRead}, init);
}

std::unique_ptr<FileProtocol> TestProtocolFactory_FileWrite(
    const CommonProtocolTestInit& init) {
  return TestProtocolFactory(
      {FileProtocolFlag::kInfo, FileProtocolFlag::kFileWrite}, init);
}

INSTANTIATE_TEST_SUITE_P(
    TestProtocolTest, CommonProtocolTest,
    Values(TestProtocolFactory_All, TestProtocolFactory_Info,
           TestProtocolFactory_List, TestProtocolFactory_FolderCreate,
           TestProtocolFactory_FileCreate, TestProtocolFactory_FileRead,
           TestProtocolFactory_FileWrite));

TEST(TestProtocolTest, BasicInitialization) {
  FileProtocolFlags test_flags = {FileProtocolFlag::kList,
                                  FileProtocolFlag::kFileCreate};
  std::vector<std::string> test_default_names = {"one", "two"};
  TestProtocol::State state;
  state.flags = test_flags;
  state.default_names = test_default_names;
  {
    TestProtocol protocol(&state);
    EXPECT_EQ(state.protocol, &protocol);
    EXPECT_EQ(protocol.GetFlags(), test_flags);
    EXPECT_EQ(protocol.GetDefaultNames(), test_default_names);
  }
  EXPECT_EQ(state.protocol, nullptr);
}

TEST(TestProtocolTest, GetPathInfo) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);
  PathInfo info;

  info = protocol.GetPathInfo("test", "");
  EXPECT_EQ(info.type, PathType::kInvalid);
  EXPECT_EQ(info.size, 0);
  EXPECT_EQ(state.invalid_call_count, 1);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kInfo;
  info = protocol.GetPathInfo("test", "/");
  EXPECT_EQ(info.type, PathType::kInvalid);
  EXPECT_EQ(info.size, 0);
  EXPECT_EQ(state.invalid_call_count, 1);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kInfo;

  info = protocol.GetPathInfo("test", "/");
  EXPECT_EQ(info.type, PathType::kFolder);
  EXPECT_EQ(info.size, 0);
  EXPECT_EQ(state.invalid_call_count, 0);
  state.ResetCounts();

  info = protocol.GetPathInfo("test", "file");
  EXPECT_EQ(info.type, PathType::kInvalid);
  EXPECT_EQ(info.size, 0);
  EXPECT_EQ(state.invalid_call_count, 1);
  state.ResetCounts();

  info = protocol.GetPathInfo("test", "/file");
  EXPECT_EQ(info.type, PathType::kFile);
  EXPECT_EQ(info.size, 10);
  EXPECT_EQ(state.invalid_call_count, 0);
  state.ResetCounts();

  info = protocol.GetPathInfo("test", "/folder");
  EXPECT_EQ(info.type, PathType::kFolder);
  EXPECT_EQ(info.size, 0);
  EXPECT_EQ(state.invalid_call_count, 0);
  state.ResetCounts();

  info = protocol.GetPathInfo("test", "/invalid");
  EXPECT_EQ(info.type, PathType::kInvalid);
  EXPECT_EQ(info.size, 0);
  EXPECT_EQ(state.invalid_call_count, 0);
  state.ResetCounts();

  info = protocol.GetPathInfo("", "/file");
  EXPECT_EQ(info.type, PathType::kInvalid);
  EXPECT_EQ(info.size, 0);
  EXPECT_EQ(state.invalid_call_count, 1);
  state.ResetCounts();

  info = protocol.GetPathInfo("other", "/file");
  EXPECT_EQ(info.type, PathType::kInvalid);
  EXPECT_EQ(info.size, 0);
  EXPECT_EQ(state.invalid_call_count, 1);
  state.ResetCounts();

  state.name.clear();
  info = protocol.GetPathInfo("other", "/file");
  EXPECT_EQ(info.type, PathType::kFile);
  EXPECT_EQ(info.size, 10);
  EXPECT_EQ(state.invalid_call_count, 0);
  state.ResetCounts();
}

TEST(TestProtocolTest, List) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);
  std::vector<std::string> paths;

  paths = protocol.List("test", "", {}, FolderMode::kNormal, kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 0);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kList;
  paths = protocol.List("test", "/", {}, FolderMode::kNormal, kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 0);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kList;

  paths =
      protocol.List("test", "folder", {}, FolderMode::kNormal, kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 0);
  state.ResetCounts();

  paths = protocol.List("other", "/", {}, FolderMode::kNormal, kAllPathTypes);
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.list_count, 1);
  EXPECT_EQ(state.basic_list_count, 0);
  state.ResetCounts();
}

TEST(TestProtocolTest, CreateFolder) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_FALSE(
      protocol.CreateFolder("test", "new-folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 0);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFolderCreate;
  EXPECT_FALSE(
      protocol.CreateFolder("test", "/new-folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 0);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFolderCreate;

  EXPECT_FALSE(
      protocol.CreateFolder("other", "/new-folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.create_folder_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 0);
  state.ResetCounts();
}

TEST(TestProtocolTest, DeleteFolder) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.DeleteFolder("test", "folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFolderCreate;
  EXPECT_FALSE(protocol.DeleteFolder("test", "/folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFolderCreate;

  EXPECT_FALSE(protocol.DeleteFolder("other", "/folder", FolderMode::kNormal));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.delete_folder_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 0);
  state.ResetCounts();
}

TEST(TestProtocolTest, CopyFolder) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/file"] = TestProtocol::PathState::NewFile("1234567890");
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.CopyFolder("test", "folder", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFolder("test", "/folder", "new-folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFolderCreate;
  EXPECT_FALSE(protocol.CopyFolder("test", "/folder", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFolderCreate;

  EXPECT_FALSE(protocol.CopyFolder("other", "/folder", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.copy_folder_count, 1);
  EXPECT_EQ(state.copy_file_count, 0);
  state.ResetCounts();
}

TEST(TestProtocolTest, DeleteFile) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.DeleteFile("test", "file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.delete_file_count, 1);
  EXPECT_EQ(state.basic_delete_file_count, 0);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFileCreate;
  EXPECT_FALSE(protocol.DeleteFile("test", "/file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.delete_file_count, 1);
  EXPECT_EQ(state.basic_delete_file_count, 0);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFileCreate;

  EXPECT_FALSE(protocol.DeleteFile("other", "/file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.delete_file_count, 1);
  EXPECT_EQ(state.basic_delete_file_count, 0);
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").type, PathType::kFile);
  state.ResetCounts();
}

TEST(TestProtocolTest, CopyFile) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.CopyFile("test", "file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  EXPECT_FALSE(protocol.CopyFile("test", "/file", "new-file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFileCreate;
  EXPECT_FALSE(protocol.CopyFile("test", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFileCreate;

  EXPECT_FALSE(protocol.CopyFile("other", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.copy_file_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 0);
  state.ResetCounts();
}

TEST(TestProtocolTest, OpenFile) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);
  std::unique_ptr<RawFile> file;

  file = protocol.OpenFile("test", "file", kReadWriteFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 0);
  file.reset();
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFileRead;
  file = protocol.OpenFile("test", "/file", FileFlag::kRead);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 0);
  file.reset();
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFileRead;

  state.flags -= FileProtocolFlag::kFileWrite;
  file = protocol.OpenFile("test", "/file", FileFlag::kWrite);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 0);
  file.reset();
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFileWrite;

  file = protocol.OpenFile("test", "/file", {FileFlag::kReset});
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 0);
  file.reset();
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFileCreate;
  file = protocol.OpenFile("test", "/new-file", kNewFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 0);
  file.reset();
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFileCreate;

  file = protocol.OpenFile("other", "/file", kReadWriteFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.open_file_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 0);
  file.reset();
  state.ResetCounts();
}

TEST(TestProtocolTest, BasicList) {
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

  paths = protocol.BasicList("test", "");
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kList;
  paths = protocol.BasicList("test", "/");
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kList;

  paths = protocol.BasicList("test", "/");
  EXPECT_EQ(paths, std::vector<std::string>({"test:/file", "test:/folder"}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.BasicList("test", "/folder");
  EXPECT_EQ(paths, std::vector<std::string>(
                       {"test:/folder/empty", "test:/folder/file-1",
                        "test:/folder/file-2", "test:/folder/subfolder"}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.BasicList("test", "/folder/empty");
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.BasicList("test", "/folder/subfolder");
  EXPECT_EQ(paths, std::vector<std::string>({"test:/folder/subfolder/file-1",
                                             "test:/folder/subfolder/file-2"}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.BasicList("test", "/file");
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.BasicList("test", "file");
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.BasicList("test", "/invalid");
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  paths = protocol.BasicList("other", "/folder");
  EXPECT_EQ(paths, std::vector<std::string>());
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();

  state.name.clear();
  paths = protocol.BasicList("other", "/folder");
  EXPECT_EQ(paths, std::vector<std::string>(
                       {"other:/folder/empty", "other:/folder/file-1",
                        "other:/folder/file-2", "other:/folder/subfolder"}));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_list_count, 1);
  state.ResetCounts();
}

TEST(TestProtocolTest, BasicCreateFolder) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.BasicCreateFolder("test", "/file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicCreateFolder("test", "/folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicCreateFolder("test", "/"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicCreateFolder("test", "new-folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicCreateFolder("test", "/file/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicCreateFolder("test", "/invalid/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFolderCreate;
  EXPECT_FALSE(protocol.BasicCreateFolder("test", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFolderCreate;

  state.fail_path = "/new-folder";
  EXPECT_FALSE(protocol.BasicCreateFolder("test", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  state.ResetCounts();
  state.fail_path.clear();

  EXPECT_TRUE(protocol.BasicCreateFolder("test", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  EXPECT_EQ(state.paths["/new-folder"].GetType(), PathType::kFolder);
  state.ResetCounts();

  EXPECT_TRUE(protocol.BasicCreateFolder("test", "/folder/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  EXPECT_EQ(state.paths["/folder/new-folder"].GetType(), PathType::kFolder);
  state.ResetCounts();

  state.paths.erase("/new-folder");
  EXPECT_FALSE(protocol.BasicCreateFolder("other", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  state.ResetCounts();

  state.name.clear();
  EXPECT_TRUE(protocol.BasicCreateFolder("other", "/new-folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_create_folder_count, 1);
  EXPECT_EQ(state.paths["/new-folder"].GetType(), PathType::kFolder);
  state.ResetCounts();
}

TEST(TestProtocolTest, BasicDeleteFolder) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder/empty"] = TestProtocol::PathState::NewFolder();
  state.paths["/empty"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.BasicDeleteFolder("test", "/"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicDeleteFolder("test", "/file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicDeleteFolder("test", "/folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicDeleteFolder("test", "/invalid"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicDeleteFolder("test", "empty"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFolderCreate;
  EXPECT_FALSE(protocol.BasicDeleteFolder("test", "/empty"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFolderCreate;

  state.fail_path = "/empty";
  EXPECT_FALSE(protocol.BasicDeleteFolder("test", "/empty"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  state.ResetCounts();
  state.fail_path.clear();

  EXPECT_TRUE(protocol.BasicDeleteFolder("test", "/empty"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  EXPECT_EQ(state.paths["/empty"].GetType(), PathType::kInvalid);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicDeleteFolder("other", "/folder/empty"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  state.ResetCounts();

  state.name.clear();
  EXPECT_TRUE(protocol.BasicDeleteFolder("other", "/folder/empty"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_delete_folder_count, 1);
  EXPECT_EQ(state.paths["/folder/empty"].GetType(), PathType::kInvalid);
  state.ResetCounts();
}

TEST(TestProtocolTest, BasicCopyFile) {
  TestProtocol::State state;
  state.implement_copy = true;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);

  EXPECT_FALSE(protocol.BasicCopyFile("test", "/folder", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(protocol.GetPathInfo("test", "/folder").type, PathType::kFolder);
  EXPECT_EQ(protocol.GetPathInfo("test", "/new-file").type, PathType::kInvalid);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicCopyFile("test", "/invalid", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(protocol.GetPathInfo("test", "/new-file").type, PathType::kInvalid);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicCopyFile("test", "file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "new-file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFileCreate;
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFileCreate;

  state.fail_path = "/file";
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  state.ResetCounts();
  state.fail_path.clear();

  state.fail_path = "/new-file";
  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  state.ResetCounts();
  state.fail_path.clear();

  EXPECT_TRUE(protocol.BasicCopyFile("test", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").type, PathType::kFile);
  EXPECT_EQ(protocol.GetPathInfo("test", "/new-file").type, PathType::kFile);
  EXPECT_EQ(state.paths["/file"].GetContents(),
            state.paths["/new-file"].GetContents());
  state.ResetCounts();

  state.paths["/new-file"].SetContents("different contents");
  EXPECT_TRUE(protocol.BasicCopyFile("test", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").type, PathType::kFile);
  EXPECT_EQ(protocol.GetPathInfo("test", "/new-file").type, PathType::kFile);
  EXPECT_EQ(state.paths["/file"].GetContents(),
            state.paths["/new-file"].GetContents());
  state.ResetCounts();

  EXPECT_FALSE(protocol.BasicCopyFile("test", "/file", "/folder"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  state.ResetCounts();

  state.paths.erase("/new-file");
  EXPECT_FALSE(protocol.BasicCopyFile("other", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  state.ResetCounts();

  state.name.clear();
  EXPECT_TRUE(protocol.BasicCopyFile("other", "/file", "/new-file"));
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_copy_file_count, 1);
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").type, PathType::kFile);
  EXPECT_EQ(protocol.GetPathInfo("test", "/new-file").type, PathType::kFile);
  EXPECT_EQ(state.paths["/file"].GetContents(),
            state.paths["/new-file"].GetContents());
  state.ResetCounts();
}

TEST(TestProtocolTest, BasicOpenFile) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  TestProtocol protocol(&state);
  std::unique_ptr<RawFile> file;

  file = protocol.BasicOpenFile("test", "/invalid", kReadWriteFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();

  file = protocol.BasicOpenFile("test", "/folder", kReadWriteFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();

  file = protocol.BasicOpenFile("test", "file", kReadWriteFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFileRead;
  file = protocol.BasicOpenFile("test", "/file", FileFlag::kRead);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFileRead;

  state.flags -= FileProtocolFlag::kFileWrite;
  file = protocol.BasicOpenFile("test", "/file", FileFlag::kWrite);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFileWrite;

  state.fail_path = "/file";
  file = protocol.BasicOpenFile("test", "/file", kReadWriteFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();
  state.fail_path.clear();

  state.open_fail_path = "/file";
  file = protocol.BasicOpenFile("test", "/file", kReadWriteFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();
  state.open_fail_path.clear();

  file = protocol.BasicOpenFile("test", "/file", kReadWriteFileFlags);
  EXPECT_NE(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_open_file_count, 1);
  EXPECT_EQ(state.paths["/file"].GetContents(), "1234567890");
  file.reset();
  state.ResetCounts();

  file = protocol.BasicOpenFile("test", "/file", kNewFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();

  file = protocol.BasicOpenFile("test", "/file", {FileFlag::kReset});
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();

  file = protocol.BasicOpenFile("test", "/file",
                                {FileFlag::kWrite, FileFlag::kReset});
  EXPECT_NE(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_open_file_count, 1);
  EXPECT_EQ(state.paths["/file"].GetContents(), "");
  file.reset();
  state.ResetCounts();

  file = protocol.BasicOpenFile("test", "/invalid/file", kNewFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();

  state.flags -= FileProtocolFlag::kFileCreate;
  file = protocol.BasicOpenFile("test", "/folder/file", kNewFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();
  state.flags += FileProtocolFlag::kFileCreate;

  file = protocol.BasicOpenFile("test", "/folder/file", kNewFileFlags);
  EXPECT_NE(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_open_file_count, 1);
  EXPECT_EQ(state.paths["/folder/file"].GetType(), PathType::kFile);
  EXPECT_EQ(state.paths["/folder/file"].GetContents(), "");
  file.reset();
  state.ResetCounts();

  file = protocol.BasicOpenFile("test", "/new-file", kNewFileFlags);
  EXPECT_NE(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_open_file_count, 1);
  EXPECT_EQ(state.paths["/new-file"].GetType(), PathType::kFile);
  EXPECT_EQ(state.paths["/new-file"].GetContents(), "");
  file.reset();
  state.ResetCounts();

  file = protocol.BasicOpenFile("other", "/file", kReadWriteFileFlags);
  EXPECT_EQ(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 1);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();

  state.name.clear();
  file = protocol.BasicOpenFile("other", "/file", kReadWriteFileFlags);
  EXPECT_NE(file, nullptr);
  EXPECT_EQ(state.invalid_call_count, 0);
  EXPECT_EQ(state.basic_open_file_count, 1);
  file.reset();
  state.ResetCounts();
}

TEST(TestProtocolTest, RawFileOpenClose) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  TestProtocol protocol(&state);

  auto file_state = state.paths["/file"].GetFile();
  ASSERT_NE(file_state, nullptr);
  EXPECT_EQ(file_state->file, nullptr);

  auto file = protocol.BasicOpenFile("test", "/file", kReadWriteFileFlags);
  EXPECT_EQ(file_state->file, file.get());
  EXPECT_EQ(file_state->flags, kReadWriteFileFlags);
  EXPECT_EQ(file_state->position, 0);
  EXPECT_EQ(file_state->contents, "1234567890");

  file.reset();
  EXPECT_EQ(file_state->file, nullptr);
  EXPECT_EQ(file_state->flags, kReadWriteFileFlags);
  EXPECT_EQ(file_state->position, 0);
  EXPECT_EQ(file_state->contents, "1234567890");
}

TEST(TestProtocolTest, RawFileSeek) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  TestProtocol protocol(&state);
  auto file_state = state.paths["/file"].GetFile();
  auto file = protocol.BasicOpenFile("test", "/file", kReadWriteFileFlags);

  EXPECT_EQ(file->SeekEnd(), 10);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->seek_end_count, 1);
  EXPECT_EQ(file_state->position, 10);
  file_state->ResetCounts();

  EXPECT_EQ(file->SeekTo(5), 5);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->seek_to_count, 1);
  EXPECT_EQ(file_state->position, 5);
  file_state->ResetCounts();

  EXPECT_EQ(file->SeekTo(11), 10);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->seek_to_count, 1);
  EXPECT_EQ(file_state->position, 10);
  file_state->ResetCounts();

  EXPECT_EQ(file->SeekTo(-1), 0);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->seek_to_count, 1);
  EXPECT_EQ(file_state->position, 0);
  file_state->ResetCounts();

  file_state->position = -1;
  EXPECT_EQ(file->SeekEnd(), -1);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->seek_end_count, 1);
  EXPECT_EQ(file_state->position, -1);
  file_state->ResetCounts();

  EXPECT_EQ(file->SeekTo(5), -1);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->seek_to_count, 1);
  EXPECT_EQ(file_state->position, -1);
  file_state->ResetCounts();
}

TEST(TestProtocolTest, RawFileWrite) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  TestProtocol protocol(&state);
  auto file_state = state.paths["/file"].GetFile();
  auto file = protocol.BasicOpenFile("test", "/file", kReadWriteFileFlags);

  EXPECT_EQ(file->Write(nullptr, 0), 0);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->write_count, 1);
  EXPECT_EQ(file_state->request_bytes_written, 0);
  EXPECT_EQ(file_state->bytes_written, 0);
  EXPECT_EQ(file_state->position, 0);
  EXPECT_EQ(file_state->contents, "1234567890");
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").size, 10);
  file_state->ResetCounts();

  EXPECT_EQ(file->Write("abcde", 5), 5);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->write_count, 1);
  EXPECT_EQ(file_state->request_bytes_written, 5);
  EXPECT_EQ(file_state->bytes_written, 5);
  EXPECT_EQ(file_state->position, 5);
  EXPECT_EQ(file_state->contents, "abcde67890");
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").size, 10);
  file_state->ResetCounts();

  EXPECT_EQ(file->Write("XYZ", 3), 3);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->write_count, 1);
  EXPECT_EQ(file_state->request_bytes_written, 3);
  EXPECT_EQ(file_state->bytes_written, 3);
  EXPECT_EQ(file_state->position, 8);
  EXPECT_EQ(file_state->contents, "abcdeXYZ90");
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").size, 10);
  file_state->ResetCounts();

  EXPECT_EQ(file->Write("...", 3), 3);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->write_count, 1);
  EXPECT_EQ(file_state->request_bytes_written, 3);
  EXPECT_EQ(file_state->bytes_written, 3);
  EXPECT_EQ(file_state->position, 11);
  EXPECT_EQ(file_state->contents, "abcdeXYZ...");
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").size, 11);
  file_state->ResetCounts();

  EXPECT_EQ(file->Write("[<>]", 4), 4);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->write_count, 1);
  EXPECT_EQ(file_state->request_bytes_written, 4);
  EXPECT_EQ(file_state->bytes_written, 4);
  EXPECT_EQ(file_state->position, 15);
  EXPECT_EQ(file_state->contents, "abcdeXYZ...[<>]");
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").size, 15);
  file_state->ResetCounts();

  EXPECT_EQ(file->SeekTo(2), 2);
  EXPECT_EQ(file->Write("--", 2), 2);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->write_count, 1);
  EXPECT_EQ(file_state->request_bytes_written, 2);
  EXPECT_EQ(file_state->bytes_written, 2);
  EXPECT_EQ(file_state->position, 4);
  EXPECT_EQ(file_state->contents, "ab--eXYZ...[<>]");
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").size, 15);
  file_state->ResetCounts();

  file_state->flags -= FileFlag::kWrite;
  EXPECT_EQ(file->Write(":::", 3), 0);
  EXPECT_EQ(file_state->invalid_call_count, 1);
  EXPECT_EQ(file_state->write_count, 1);
  EXPECT_EQ(file_state->request_bytes_written, 3);
  EXPECT_EQ(file_state->bytes_written, 0);
  EXPECT_EQ(file_state->position, 4);
  EXPECT_EQ(file_state->contents, "ab--eXYZ...[<>]");
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").size, 15);
  file_state->ResetCounts();
  file_state->flags += FileFlag::kWrite;

  file_state->position = -1;
  EXPECT_EQ(file->Write(":::", 3), 0);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->write_count, 1);
  EXPECT_EQ(file_state->request_bytes_written, 3);
  EXPECT_EQ(file_state->bytes_written, 0);
  EXPECT_EQ(file_state->position, -1);
  EXPECT_EQ(file_state->contents, "ab--eXYZ...[<>]");
  EXPECT_EQ(protocol.GetPathInfo("test", "/file").size, 15);
  file_state->ResetCounts();
}

TEST(TestProtocolTest, RawFileRead) {
  TestProtocol::State state;
  state.name = "test";
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  TestProtocol protocol(&state);
  auto file_state = state.paths["/file"].GetFile();
  auto file = protocol.BasicOpenFile("test", "/file", kReadWriteFileFlags);
  char buffer[20];

  std::memset(buffer, 0, sizeof(buffer));
  EXPECT_EQ(file->Read(buffer, 0), 0);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->read_count, 1);
  EXPECT_EQ(file_state->request_bytes_read, 0);
  EXPECT_EQ(file_state->bytes_read, 0);
  EXPECT_EQ(file_state->position, 0);
  EXPECT_STREQ(buffer, "");
  file_state->ResetCounts();

  std::memset(buffer, 0, sizeof(buffer));
  EXPECT_EQ(file->Read(buffer, 5), 5);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->read_count, 1);
  EXPECT_EQ(file_state->request_bytes_read, 5);
  EXPECT_EQ(file_state->bytes_read, 5);
  EXPECT_EQ(file_state->position, 5);
  EXPECT_STREQ(buffer, "12345");
  file_state->ResetCounts();

  std::memset(buffer, 0, sizeof(buffer));
  EXPECT_EQ(file->Read(buffer, 3), 3);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->read_count, 1);
  EXPECT_EQ(file_state->request_bytes_read, 3);
  EXPECT_EQ(file_state->bytes_read, 3);
  EXPECT_EQ(file_state->position, 8);
  EXPECT_STREQ(buffer, "678");
  file_state->ResetCounts();

  std::memset(buffer, 0, sizeof(buffer));
  EXPECT_EQ(file->Read(buffer, 3), 2);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->read_count, 1);
  EXPECT_EQ(file_state->request_bytes_read, 3);
  EXPECT_EQ(file_state->bytes_read, 2);
  EXPECT_EQ(file_state->position, 10);
  EXPECT_STREQ(buffer, "90");
  file_state->ResetCounts();

  std::memset(buffer, 0, sizeof(buffer));
  EXPECT_EQ(file->Read(buffer, 3), 0);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->read_count, 1);
  EXPECT_EQ(file_state->request_bytes_read, 3);
  EXPECT_EQ(file_state->bytes_read, 0);
  EXPECT_EQ(file_state->position, 10);
  EXPECT_STREQ(buffer, "");
  file_state->ResetCounts();

  std::memset(buffer, 0, sizeof(buffer));
  EXPECT_EQ(file->SeekTo(2), 2);
  EXPECT_EQ(file->Read(buffer, 2), 2);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->read_count, 1);
  EXPECT_EQ(file_state->request_bytes_read, 2);
  EXPECT_EQ(file_state->bytes_read, 2);
  EXPECT_EQ(file_state->position, 4);
  EXPECT_STREQ(buffer, "34");
  file_state->ResetCounts();

  file_state->flags -= FileFlag::kRead;
  std::memset(buffer, 0, sizeof(buffer));
  EXPECT_EQ(file->Read(buffer, 2), 0);
  EXPECT_EQ(file_state->invalid_call_count, 1);
  EXPECT_EQ(file_state->read_count, 1);
  EXPECT_EQ(file_state->request_bytes_read, 2);
  EXPECT_EQ(file_state->bytes_read, 0);
  EXPECT_EQ(file_state->position, 4);
  EXPECT_STREQ(buffer, "");
  file_state->ResetCounts();
  file_state->flags += FileFlag::kRead;

  file_state->position = -1;
  std::memset(buffer, 0, sizeof(buffer));
  EXPECT_EQ(file->Read(buffer, 2), 0);
  EXPECT_EQ(file_state->invalid_call_count, 0);
  EXPECT_EQ(file_state->read_count, 1);
  EXPECT_EQ(file_state->request_bytes_read, 2);
  EXPECT_EQ(file_state->bytes_read, 0);
  EXPECT_EQ(file_state->position, -1);
  EXPECT_STREQ(buffer, "");
  file_state->ResetCounts();
}

}  // namespace
}  // namespace gb
