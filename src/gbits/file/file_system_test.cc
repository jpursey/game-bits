#include "gbits/file/file_system.h"

#include <algorithm>

#include "gbits/file/test_protocol.h"
#include "gbits/test/test_util.cc"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

TEST(FileSystemTest, DefaultConstruct) {
  FileSystem file_system;
  EXPECT_THAT(file_system.GetProtocolNames(), IsEmpty());
  EXPECT_EQ(file_system.GetDefaultProtocolName(), "");
}

TEST(FileSystemTest, RegisterDefaultNames) {
  TestProtocol::State state;
  FileSystem file_system;

  EXPECT_FALSE(file_system.Register(std::make_unique<TestProtocol>(&state)));
  EXPECT_EQ(state.protocol, nullptr);
  EXPECT_THAT(file_system.GetProtocolNames(), IsEmpty());

  state.default_names = {"foo", "b@r"};
  EXPECT_FALSE(file_system.Register(std::make_unique<TestProtocol>(&state)));
  EXPECT_EQ(state.protocol, nullptr);
  EXPECT_FALSE(file_system.IsRegistered(state.default_names[0]));
  EXPECT_FALSE(file_system.IsRegistered(state.default_names[1]));
  EXPECT_THAT(file_system.GetProtocolNames(), IsEmpty());

  state.default_names = {"foo", "bar"};
  EXPECT_TRUE(file_system.Register(std::make_unique<TestProtocol>(&state)));
  EXPECT_TRUE(file_system.IsRegistered(state.default_names[0]));
  EXPECT_TRUE(file_system.IsRegistered(state.default_names[1]));
  EXPECT_THAT(file_system.GetProtocolNames(),
              UnorderedElementsAreArray(state.default_names));

  file_system.List("foo:/");
  EXPECT_EQ(state.list_count, 1);
  file_system.List("bar:/");
  EXPECT_EQ(state.list_count, 2);
}

TEST(FileSystemTest, RegisterExplicitName) {
  TestProtocol::State state;
  state.default_names = {"foo", "bar"};
  FileSystem file_system;

  EXPECT_FALSE(
      file_system.Register(std::make_unique<TestProtocol>(&state), "t--t"));
  EXPECT_EQ(state.protocol, nullptr);
  EXPECT_FALSE(file_system.IsRegistered("t--t"));
  EXPECT_THAT(file_system.GetProtocolNames(), IsEmpty());

  EXPECT_FALSE(file_system.Register(std::make_unique<TestProtocol>(&state),
                                    "MixedCase"));
  EXPECT_EQ(state.protocol, nullptr);
  EXPECT_FALSE(file_system.IsRegistered("MixedCase"));
  EXPECT_THAT(file_system.GetProtocolNames(), IsEmpty());

  const std::string valid_name = "abcdefghijklmnopqrstuvwxyz0123456789";
  EXPECT_TRUE(
      file_system.Register(std::make_unique<TestProtocol>(&state), valid_name));
  EXPECT_TRUE(file_system.IsRegistered(valid_name));
  EXPECT_THAT(file_system.GetProtocolNames(), ElementsAre(valid_name));
  EXPECT_FALSE(file_system.IsRegistered(state.default_names[0]));
  EXPECT_FALSE(file_system.IsRegistered(state.default_names[1]));

  TestProtocol::State new_state;
  EXPECT_FALSE(file_system.Register(std::make_unique<TestProtocol>(&new_state),
                                    valid_name));

  file_system.List("foo:/");
  EXPECT_EQ(state.list_count, 0);
  file_system.List("bar:/");
  EXPECT_EQ(state.list_count, 0);
  file_system.List(valid_name + ":/");
  EXPECT_EQ(state.list_count, 1);
}

TEST(FileSystemTest, RegisterNullProtocol) {
  FileSystem file_system;
  EXPECT_FALSE(file_system.Register(nullptr));
  EXPECT_FALSE(file_system.Register(nullptr, "test"));
}

TEST(FileSystemTest, SetDefaultProtocol) {
  TestProtocol::State state;
  FileSystem file_system;

  EXPECT_TRUE(
      file_system.Register(std::make_unique<TestProtocol>(&state), "test"));

  EXPECT_EQ(file_system.GetDefaultProtocolName(), "");
  file_system.List("/");
  EXPECT_EQ(state.list_count, 0);

  EXPECT_FALSE(file_system.SetDefaultProtocol("foo"));

  EXPECT_TRUE(file_system.SetDefaultProtocol("test"));
  EXPECT_EQ(file_system.GetDefaultProtocolName(), "test");
  file_system.List("/");
  EXPECT_EQ(state.list_count, 1);
}

TEST(FileSystemTest, RegisterProtocolListRequiresInfo) {
  FileSystem file_system;
  TestProtocol::State state;

  state.flags = {FileProtocolFlag::kList, FileProtocolFlag::kFileRead};
  EXPECT_FALSE(
      file_system.Register(std::make_unique<TestProtocol>(&state), "test"));
  EXPECT_EQ(state.protocol, nullptr);
  EXPECT_TRUE(file_system.GetFlags("test").IsEmpty());

  state.flags = {FileProtocolFlag::kList, FileProtocolFlag::kInfo,
                 FileProtocolFlag::kFileRead};
  EXPECT_TRUE(
      file_system.Register(std::make_unique<TestProtocol>(&state), "test"));
  EXPECT_EQ(file_system.GetFlags("test"), state.flags);
}

TEST(FileSystemTest, RegisterProtocolFolderCreateRequiresFileCreate) {
  FileSystem file_system;
  TestProtocol::State state;

  state.flags = {FileProtocolFlag::kFolderCreate, FileProtocolFlag::kFileWrite};
  EXPECT_FALSE(
      file_system.Register(std::make_unique<TestProtocol>(&state), "test"));
  EXPECT_EQ(state.protocol, nullptr);
  EXPECT_TRUE(file_system.GetFlags("test").IsEmpty());

  state.flags = {FileProtocolFlag::kFolderCreate, FileProtocolFlag::kFileCreate,
                 FileProtocolFlag::kFileWrite};
  EXPECT_TRUE(
      file_system.Register(std::make_unique<TestProtocol>(&state), "test"));
  EXPECT_EQ(file_system.GetFlags("test"), state.flags);
}

TEST(FileSystemTest, RegisterProtocolFileCreateRequiresFileWrite) {
  FileSystem file_system;
  TestProtocol::State state;

  state.flags = {FileProtocolFlag::kFileCreate, FileProtocolFlag::kFileRead};
  EXPECT_FALSE(
      file_system.Register(std::make_unique<TestProtocol>(&state), "test"));
  EXPECT_EQ(state.protocol, nullptr);
  EXPECT_TRUE(file_system.GetFlags("test").IsEmpty());

  state.flags = {FileProtocolFlag::kFileCreate, FileProtocolFlag::kFileWrite};
  EXPECT_TRUE(
      file_system.Register(std::make_unique<TestProtocol>(&state), "test"));
  EXPECT_EQ(file_system.GetFlags("test"), state.flags);
}

TEST(FileSystemTest, RegisterProtocolFileReadOrWriteRequired) {
  FileSystem file_system;
  TestProtocol::State state;
  state.flags = {};
  EXPECT_FALSE(
      file_system.Register(std::make_unique<TestProtocol>(&state), "test"));
  EXPECT_EQ(state.protocol, nullptr);
  EXPECT_TRUE(file_system.GetFlags("test").IsEmpty());

  TestProtocol::State success_state_1;
  success_state_1.flags = FileProtocolFlag::kFileRead;
  EXPECT_TRUE(file_system.Register(
      std::make_unique<TestProtocol>(&success_state_1), "success1"));
  EXPECT_EQ(file_system.GetFlags("success1"), success_state_1.flags);

  TestProtocol::State success_state_2;
  success_state_2.flags = FileProtocolFlag::kFileWrite;
  EXPECT_TRUE(file_system.Register(
      std::make_unique<TestProtocol>(&success_state_2), "success2"));
  EXPECT_EQ(file_system.GetFlags("success2"), success_state_2.flags);
}

TEST(FileSystemTest, GetPathInfo) {
  TestProtocol::State state;
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  EXPECT_EQ(file_system.GetPathInfo("test:file").type, PathType::kInvalid);
  EXPECT_EQ(file_system.GetPathInfo("foo:/file").type, PathType::kInvalid);
  EXPECT_EQ(file_system.GetPathInfo("test:/file").type, PathType::kFile);
  EXPECT_EQ(file_system.GetPathInfo("test:/file").size, 10);
  EXPECT_EQ(file_system.GetPathInfo("test:/folder").type, PathType::kFolder);
  EXPECT_EQ(file_system.GetPathInfo("test:/folder").size, 0);
  EXPECT_EQ(file_system.GetPathInfo("TEST:/.\\abc///..\\folder").type,
            PathType::kFolder);

  state.flags -= FileProtocolFlag::kInfo;
  EXPECT_EQ(file_system.GetPathInfo("test:/folder").type, PathType::kInvalid);
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileSystemTest, List) {
  TestProtocol::State state;
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/file-1"] = TestProtocol::PathState::NewFile("abcde");
  state.paths["/folder/sub-folder"] = TestProtocol::PathState::NewFolder();
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");
  std::vector<std::string> result;

  EXPECT_THAT(file_system.List("test:"), IsEmpty());
  EXPECT_EQ(state.list_count, 0);
  EXPECT_THAT(file_system.List("foo:/"), IsEmpty());
  EXPECT_EQ(state.list_count, 0);
  EXPECT_THAT(file_system.List("test:/"),
              UnorderedElementsAre("test:/file", "test:/folder"));
  EXPECT_THAT(file_system.List("test:/", "fi*"),
              UnorderedElementsAre("test:/file"));
  EXPECT_THAT(file_system.List("test:/", FolderMode::kNormal),
              UnorderedElementsAre("test:/file", "test:/folder"));
  EXPECT_THAT(
      file_system.List("test:/", FolderMode::kRecursive),
      UnorderedElementsAre("test:/file", "test:/folder", "test:/folder/file-1",
                           "test:/folder/sub-folder"));
  EXPECT_THAT(file_system.List("test:/", "fi*", FolderMode::kNormal),
              UnorderedElementsAre("test:/file"));
  EXPECT_THAT(file_system.List("test:/", "fi*", FolderMode::kRecursive),
              UnorderedElementsAre("test:/file", "test:/folder/file-1"));
  EXPECT_THAT(
      file_system.List("TEST:/.\\abc///..\\folder"),
      UnorderedElementsAre("test:/folder/file-1", "test:/folder/sub-folder"));

  state.flags -= FileProtocolFlag::kList;
  EXPECT_THAT(file_system.List("test:/"), IsEmpty());
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileSystemTest, ListFolders) {
  TestProtocol::State state;
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/folder-1"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder-2"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder-1/file-1"] = TestProtocol::PathState::NewFile("abcde");
  state.paths["/folder-1/sub-folder-1"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder-1/sub-folder-2"] = TestProtocol::PathState::NewFolder();
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");
  std::vector<std::string> result;

  EXPECT_THAT(file_system.ListFolders("test:"), IsEmpty());
  EXPECT_EQ(state.list_count, 0);
  EXPECT_THAT(file_system.ListFolders("foo:/"), IsEmpty());
  EXPECT_EQ(state.list_count, 0);
  EXPECT_THAT(file_system.ListFolders("test:/"),
              UnorderedElementsAre("test:/folder-1", "test:/folder-2"));
  EXPECT_THAT(file_system.ListFolders("test:/", "*-1"),
              UnorderedElementsAre("test:/folder-1"));
  EXPECT_THAT(file_system.ListFolders("test:/", FolderMode::kNormal),
              UnorderedElementsAre("test:/folder-1", "test:/folder-2"));
  EXPECT_THAT(file_system.ListFolders("test:/", FolderMode::kRecursive),
              UnorderedElementsAre("test:/folder-1", "test:/folder-2",
                                   "test:/folder-1/sub-folder-1",
                                   "test:/folder-1/sub-folder-2"));
  EXPECT_THAT(file_system.ListFolders("test:/", "*-1", FolderMode::kNormal),
              UnorderedElementsAre("test:/folder-1"));
  EXPECT_THAT(
      file_system.ListFolders("test:/", "*-1", FolderMode::kRecursive),
      UnorderedElementsAre("test:/folder-1", "test:/folder-1/sub-folder-1"));
  EXPECT_THAT(file_system.ListFolders("TEST:/.\\abc///..\\folder-1"),
              UnorderedElementsAre("test:/folder-1/sub-folder-1",
                                   "test:/folder-1/sub-folder-2"));

  state.flags -= FileProtocolFlag::kList;
  EXPECT_THAT(file_system.ListFolders("test:/"), IsEmpty());
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileSystemTest, ListFiles) {
  TestProtocol::State state;
  state.paths["/file-1"] = TestProtocol::PathState::NewFile("1234567890");
  state.paths["/file-2"] = TestProtocol::PathState::NewFile("lmnopqrstu");
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/file-1"] = TestProtocol::PathState::NewFile("abcde");
  state.paths["/folder/file-2"] = TestProtocol::PathState::NewFile("fghij");
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");
  std::vector<std::string> result;

  EXPECT_THAT(file_system.ListFiles("test:"), IsEmpty());
  EXPECT_EQ(state.list_count, 0);
  EXPECT_THAT(file_system.ListFiles("foo:/"), IsEmpty());
  EXPECT_EQ(state.list_count, 0);
  EXPECT_THAT(file_system.ListFiles("test:/"),
              UnorderedElementsAre("test:/file-1", "test:/file-2"));
  EXPECT_THAT(file_system.ListFiles("test:/", "*-1"),
              UnorderedElementsAre("test:/file-1"));
  EXPECT_THAT(file_system.ListFiles("test:/", FolderMode::kNormal),
              UnorderedElementsAre("test:/file-1", "test:/file-2"));
  EXPECT_THAT(
      file_system.ListFiles("test:/", FolderMode::kRecursive),
      UnorderedElementsAre("test:/file-1", "test:/file-2",
                           "test:/folder/file-1", "test:/folder/file-2"));
  EXPECT_THAT(file_system.ListFiles("test:/", "*-1", FolderMode::kNormal),
              UnorderedElementsAre("test:/file-1"));
  EXPECT_THAT(file_system.ListFiles("test:/", "*-1", FolderMode::kRecursive),
              UnorderedElementsAre("test:/file-1", "test:/folder/file-1"));
  EXPECT_THAT(
      file_system.ListFiles("TEST:/.\\abc///..\\folder"),
      UnorderedElementsAre("test:/folder/file-1", "test:/folder/file-2"));

  state.flags -= FileProtocolFlag::kList;
  EXPECT_THAT(file_system.ListFiles("test:/"), IsEmpty());
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileSystemTest, CreateFolder) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  EXPECT_FALSE(file_system.CreateFolder("test:folder"));
  EXPECT_EQ(state.create_folder_count, 0);
  EXPECT_FALSE(file_system.CreateFolder("foo:/folder"));
  EXPECT_EQ(state.create_folder_count, 0);
  EXPECT_TRUE(file_system.CreateFolder("test:/folder-1"));
  EXPECT_EQ(state.paths["/folder-1"].GetType(), PathType::kFolder);
  EXPECT_TRUE(file_system.CreateFolder("test:/folder-2", FolderMode::kNormal));
  EXPECT_EQ(state.paths["/folder-2"].GetType(), PathType::kFolder);
  EXPECT_FALSE(
      file_system.CreateFolder("test:/folder-3/folder-4", FolderMode::kNormal));
  EXPECT_EQ(state.paths["/folder-3"].GetType(), PathType::kInvalid);
  EXPECT_TRUE(file_system.CreateFolder("test:/folder-5/folder-6",
                                       FolderMode::kRecursive));
  EXPECT_EQ(state.paths["/folder-5"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/folder-5/folder-6"].GetType(), PathType::kFolder);
  EXPECT_TRUE(file_system.CreateFolder("TEST:/.\\abc///..\\folder-7"));
  EXPECT_EQ(state.paths["/folder-7"].GetType(), PathType::kFolder);

  state.flags -= FileProtocolFlag::kFolderCreate;
  EXPECT_FALSE(file_system.CreateFolder("test:/folder-8"));
  EXPECT_EQ(state.paths["/folder-8"].GetType(), PathType::kInvalid);
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileSystemTest, DeleteFolder) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  EXPECT_FALSE(file_system.DeleteFolder("test:folder"));
  EXPECT_EQ(state.delete_folder_count, 0);
  EXPECT_FALSE(file_system.DeleteFolder("foo:/folder"));
  EXPECT_EQ(state.delete_folder_count, 0);

  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/file"] = TestProtocol::PathState::NewFile("12345");
  EXPECT_FALSE(file_system.DeleteFolder("test:/folder"));
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kFolder);
  EXPECT_FALSE(file_system.DeleteFolder("test:/folder", FolderMode::kNormal));
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kFolder);
  EXPECT_TRUE(file_system.DeleteFolder("test:/folder", FolderMode::kRecursive));
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kInvalid);

  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  EXPECT_TRUE(file_system.DeleteFolder("test:/folder"));
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kInvalid);

  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  EXPECT_TRUE(file_system.DeleteFolder("test:/folder", FolderMode::kNormal));
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kInvalid);

  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  EXPECT_TRUE(file_system.DeleteFolder("TEST:/.\\abc///..\\folder"));
  EXPECT_EQ(state.paths["/folder"].GetType(), PathType::kInvalid);

  state.flags -= FileProtocolFlag::kFolderCreate;
  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  EXPECT_FALSE(file_system.DeleteFolder("test:/folder"));
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileSystemTest, DeleteFile) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  EXPECT_FALSE(file_system.DeleteFile("test:file"));
  EXPECT_EQ(state.delete_file_count, 0);
  EXPECT_FALSE(file_system.DeleteFile("foo:/file"));
  EXPECT_EQ(state.delete_file_count, 0);
  EXPECT_EQ(state.paths["/file"].GetType(), PathType::kFile);
  EXPECT_TRUE(file_system.DeleteFile("test:/file"));
  EXPECT_EQ(state.paths["/file"].GetType(), PathType::kInvalid);

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  EXPECT_TRUE(file_system.DeleteFile("TEST:/.\\abc///..\\file"));
  EXPECT_EQ(state.paths["/file"].GetType(), PathType::kInvalid);

  state.flags -= FileProtocolFlag::kFileCreate;
  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  EXPECT_FALSE(file_system.DeleteFile("test:/file"));
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileSystemTest, CopyFolder) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/sub-folder"] = TestProtocol::PathState::NewFolder();
  state.paths["/folder/file"] = TestProtocol::PathState::NewFile("1234567890");

  EXPECT_FALSE(file_system.CopyFolder("test:folder", "test:/new-folder"));
  EXPECT_EQ(state.copy_folder_count, 0);
  EXPECT_FALSE(file_system.CopyFolder("test:/folder", "test:new-folder"));
  EXPECT_EQ(state.copy_folder_count, 0);
  EXPECT_FALSE(file_system.CopyFolder("foo:/folder", "test:/new-folder"));
  EXPECT_EQ(state.copy_folder_count, 0);
  EXPECT_FALSE(file_system.CopyFolder("test:/folder", "foo:/new-folder"));
  EXPECT_EQ(state.copy_folder_count, 0);
  EXPECT_FALSE(file_system.CopyFolder("test:/", "test:/new-folder"));
  EXPECT_EQ(state.copy_folder_count, 0);
  EXPECT_FALSE(file_system.CopyFolder("test:/folder", "test:/folder"));
  EXPECT_EQ(state.copy_folder_count, 0);
  EXPECT_FALSE(
      file_system.CopyFolder("test:/folder", "test:/folder/sub-folder"));
  EXPECT_EQ(state.copy_folder_count, 0);
  EXPECT_TRUE(file_system.CopyFolder("test:/folder", "test:/new-folder"));
  EXPECT_EQ(state.paths["/new-folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder/sub-folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder/file"].GetType(), PathType::kFile);
  EXPECT_TRUE(file_system.CopyFolder("TEST:/.\\abc///..\\folder",
                                     "TEST:/.\\abc///..\\new-folder-2"));
  EXPECT_EQ(state.paths["/new-folder-2"].GetType(), PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder-2/sub-folder"].GetType(),
            PathType::kFolder);
  EXPECT_EQ(state.paths["/new-folder-2/file"].GetType(), PathType::kFile);

  state.flags -= FileProtocolFlag::kFolderCreate;
  EXPECT_FALSE(file_system.CopyFolder("test:/folder", "test:/new-folder-3"));
  EXPECT_EQ(state.paths["/new-folder-3"].GetType(), PathType::kInvalid);
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileSystemTest, CopyFolderAcrossProtocols) {
  TestProtocol::State state[2];
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state[0]), "test0");
  file_system.Register(std::make_unique<TestProtocol>(&state[1]), "test1");

  state[0].paths["/file"] = TestProtocol::PathState::NewFile("lmnopqrstu");
  state[0].paths["/folder"] = TestProtocol::PathState::NewFolder();
  state[0].paths["/folder/file"] =
      TestProtocol::PathState::NewFile("1234567890");
  state[0].paths["/folder/sub-folder"] = TestProtocol::PathState::NewFolder();
  state[0].paths["/folder/sub-folder/file"] =
      TestProtocol::PathState::NewFile("abcdef");

  state[0].flags -= FileProtocolFlag::kList;
  EXPECT_FALSE(file_system.CopyFolder("test0:/folder", "test1:/folder"));
  EXPECT_EQ(state[0].list_count, 0);
  state[0].flags += FileProtocolFlag::kList;

  state[0].flags -= FileProtocolFlag::kFileRead;
  EXPECT_FALSE(file_system.CopyFolder("test0:/folder", "test1:/folder"));
  EXPECT_EQ(state[0].list_count, 0);
  state[0].flags += FileProtocolFlag::kFileRead;

  state[1].flags -= FileProtocolFlag::kFolderCreate;
  EXPECT_FALSE(file_system.CopyFolder("test0:/folder", "test1:/folder"));
  EXPECT_EQ(state[0].list_count, 0);
  state[1].flags += FileProtocolFlag::kFolderCreate;

  state[0].flags = {FileProtocolFlag::kInfo, FileProtocolFlag::kList,
                    FileProtocolFlag::kFileRead};
  state[1].flags = {FileProtocolFlag::kInfo, FileProtocolFlag::kFolderCreate,
                    FileProtocolFlag::kFileCreate,
                    FileProtocolFlag::kFileWrite};
  EXPECT_TRUE(file_system.CopyFolder("test0:/folder", "test1:/folder"));
  EXPECT_EQ(state[1].paths["/folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state[1].paths["/folder/file"].GetContents(),
            state[0].paths["/folder/file"].GetContents());
  EXPECT_EQ(state[1].paths["/folder/sub-folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state[1].paths["/folder/sub-folder/file"].GetContents(),
            state[0].paths["/folder/sub-folder/file"].GetContents());

  state[1].paths.clear();
  state[1].fail_path = "/folder/sub-folder";
  EXPECT_FALSE(file_system.CopyFolder("test0:/folder", "test1:/folder"));
  EXPECT_EQ(state[1].paths["/folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state[1].paths["/folder/file"].GetContents(),
            state[0].paths["/folder/file"].GetContents());
  EXPECT_EQ(state[1].paths["/folder/sub-folder"].GetType(), PathType::kInvalid);

  state[1].paths.clear();
  state[1].fail_path = "/folder/file";
  EXPECT_FALSE(file_system.CopyFolder("test0:/folder", "test1:/folder"));
  EXPECT_EQ(state[1].paths["/folder"].GetType(), PathType::kFolder);
  EXPECT_EQ(state[1].paths["/folder/file"].GetType(), PathType::kInvalid);

  EXPECT_EQ(state[0].invalid_call_count, 0);
  EXPECT_EQ(state[1].invalid_call_count, 0);
}

TEST(FileSystemTest, CopyFile) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");

  EXPECT_FALSE(file_system.CopyFile("test:file", "test:/new-file"));
  EXPECT_EQ(state.copy_file_count, 0);
  EXPECT_FALSE(file_system.CopyFile("test:/file", "test:new-file"));
  EXPECT_EQ(state.copy_file_count, 0);
  EXPECT_FALSE(file_system.CopyFile("foo:/file", "test:/new-file"));
  EXPECT_EQ(state.copy_file_count, 0);
  EXPECT_FALSE(file_system.CopyFile("test:/file", "foo:/new-file"));
  EXPECT_EQ(state.copy_file_count, 0);
  EXPECT_TRUE(file_system.CopyFile("test:/file", "test:/new-file"));
  EXPECT_EQ(state.paths["/new-file"].GetContents(),
            state.paths["/file"].GetContents());
  EXPECT_TRUE(file_system.CopyFile("TEST:/.\\abc///..\\file",
                                   "TEST:/.\\abc///..\\new-file-2"));
  EXPECT_EQ(state.paths["/new-file-2"].GetContents(),
            state.paths["/file"].GetContents());

  state.flags -= FileProtocolFlag::kFileCreate;
  EXPECT_FALSE(file_system.CopyFile("test:/file", "test:/new-file-3"));
  EXPECT_EQ(state.paths["/new-file-3"].GetType(), PathType::kInvalid);
  EXPECT_EQ(state.invalid_call_count, 0);
}

TEST(FileSystemTest, CopyFilesAcrossProtocols) {
  TestProtocol::State state[2];
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state[0]), "test0");
  file_system.Register(std::make_unique<TestProtocol>(&state[1]), "test1");

  state[0].paths["/file"] = TestProtocol::PathState::NewFile("1234567890");

  state[0].flags -= FileProtocolFlag::kFileRead;
  EXPECT_FALSE(file_system.CopyFile("test0:/file", "test1:/file"));
  EXPECT_EQ(state[0].open_file_count, 0);
  EXPECT_EQ(state[1].open_file_count, 0);
  state[0].flags += FileProtocolFlag::kFileRead;

  state[1].flags -= FileProtocolFlag::kFileWrite;
  EXPECT_FALSE(file_system.CopyFile("test0:/file", "test1:/file"));
  EXPECT_EQ(state[0].open_file_count, 0);
  EXPECT_EQ(state[1].open_file_count, 0);
  state[1].flags += FileProtocolFlag::kFileWrite;

  state[0].open_fail_path = "/file";
  EXPECT_FALSE(file_system.CopyFile("test0:/file", "test1:/file"));
  EXPECT_EQ(state[0].open_file_count, 1);
  EXPECT_EQ(state[1].open_file_count, 0);
  state[0].open_fail_path.clear();
  state[0].ResetCounts();

  state[1].open_fail_path = "/file";
  EXPECT_FALSE(file_system.CopyFile("test0:/file", "test1:/file"));
  EXPECT_EQ(state[0].open_file_count, 1);
  EXPECT_EQ(state[1].open_file_count, 1);
  EXPECT_EQ(state[1].paths["/file"].GetType(), PathType::kInvalid);
  state[1].open_fail_path.clear();
  state[0].ResetCounts();
  state[1].ResetCounts();

  state[1].io_fail_path = "/file";
  EXPECT_FALSE(file_system.CopyFile("test0:/file", "test1:/file"));
  EXPECT_EQ(state[0].open_file_count, 1);
  EXPECT_EQ(state[1].open_file_count, 1);
  EXPECT_EQ(state[1].paths["/file"].GetType(), PathType::kFile);
  EXPECT_THAT(state[1].paths["/file"].GetContents(), IsEmpty());
  state[1].io_fail_path.clear();
  state[0].ResetCounts();
  state[1].ResetCounts();

  EXPECT_TRUE(file_system.CopyFile("test0:/file", "test1:/file"));
  EXPECT_EQ(state[1].paths["/file"].GetType(), PathType::kFile);
  EXPECT_EQ(state[1].paths["/file"].GetContents(),
            state[0].paths["/file"].GetContents());

  // Generate a random string long enough to require multiple read/write
  // operations. True randomness is unimportant here, but we make sure that no
  // two adjacent characters are equal to better detect off-by-one errors.
  std::string text = GenerateTestString(FileSystem::kCopyBufferSize * 3);
  state[0].paths["/file"] = TestProtocol::PathState::NewFile(
      text.substr(0, FileSystem::kCopyBufferSize));
  state[1].paths.clear();
  EXPECT_TRUE(file_system.CopyFile("test0:/file", "test1:/file"));
  EXPECT_EQ(state[1].paths["/file"].GetContents(),
            state[0].paths["/file"].GetContents());

  state[0].paths["/file"] = TestProtocol::PathState::NewFile(
      text.substr(0, FileSystem::kCopyBufferSize + 1));
  state[1].paths.clear();
  EXPECT_TRUE(file_system.CopyFile("test0:/file", "test1:/file"));
  EXPECT_EQ(state[1].paths["/file"].GetContents(),
            state[0].paths["/file"].GetContents());

  state[0].paths["/file"] = TestProtocol::PathState::NewFile(
      text.substr(0, FileSystem::kCopyBufferSize * 2));
  state[1].paths.clear();
  EXPECT_TRUE(file_system.CopyFile("test0:/file", "test1:/file"));
  EXPECT_EQ(state[1].paths["/file"].GetContents(),
            state[0].paths["/file"].GetContents());

  state[0].paths["/file"] = TestProtocol::PathState::NewFile(
      text.substr(0, FileSystem::kCopyBufferSize * 2 + 1));
  state[1].paths.clear();
  EXPECT_TRUE(file_system.CopyFile("test0:/file", "test1:/file"));
  EXPECT_EQ(state[1].paths["/file"].GetContents(),
            state[0].paths["/file"].GetContents());

  EXPECT_EQ(state[0].invalid_call_count, 0);
  EXPECT_EQ(state[1].invalid_call_count, 0);
}

TEST(FileSystemTest, OpenFile) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");

  EXPECT_EQ(file_system.OpenFile("test:file", FileFlag::kRead), nullptr);
  EXPECT_EQ(file_system.OpenFile("foo:/file", FileFlag::kRead), nullptr);
  EXPECT_EQ(file_system.OpenFile("test:/file", {}), nullptr);
  EXPECT_EQ(
      file_system.OpenFile("test:/file", {FileFlag::kRead, FileFlag::kReset}),
      nullptr);
  EXPECT_EQ(
      file_system.OpenFile("test:/file", {FileFlag::kRead, FileFlag::kCreate}),
      nullptr);

  state.flags -= FileProtocolFlag::kFileRead;
  EXPECT_EQ(file_system.OpenFile("test:/file", FileFlag::kRead), nullptr);
  state.flags += FileProtocolFlag::kFileRead;

  state.flags -= FileProtocolFlag::kFileWrite;
  EXPECT_EQ(file_system.OpenFile("test:/file", FileFlag::kWrite), nullptr);
  state.flags += FileProtocolFlag::kFileWrite;

  state.flags -= FileProtocolFlag::kFileCreate;
  EXPECT_EQ(
      file_system.OpenFile("test:/file", {FileFlag::kCreate, FileFlag::kWrite}),
      nullptr);
  state.flags += FileProtocolFlag::kFileCreate;

  state.flags = FileProtocolFlag::kFileRead;
  EXPECT_NE(file_system.OpenFile("test:/file", FileFlag::kRead), nullptr);

  state.flags = FileProtocolFlag::kFileWrite;
  EXPECT_NE(
      file_system.OpenFile("test:/file", {FileFlag::kReset, FileFlag::kWrite}),
      nullptr);

  state.flags = {FileProtocolFlag::kFileCreate, FileProtocolFlag::kFileWrite};
  EXPECT_NE(
      file_system.OpenFile("test:/file", {FileFlag::kCreate, FileFlag::kReset,
                                          FileFlag::kWrite}),
      nullptr);
}

}  // namespace
}  // namespace gb
