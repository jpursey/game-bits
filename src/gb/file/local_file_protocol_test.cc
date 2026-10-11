// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/local_file_protocol.h"

#include <filesystem>
#include <fstream>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "gb/base/context_builder.h"
#include "gb/base/scoped_call.h"
#include "gb/file/common_protocol_test.h"
#include "gb/file/file_system.h"
#include "gb/file/path.h"
#include "gb/file/raw_file.h"
#include "gb/test/test_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

namespace fs = std::filesystem;

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using ::testing::Values;

// Converts between UTF-8 paths used by LocalFileProtocol and std::filesystem
// paths.
fs::path ToFsPath(std::string_view path) {
  return fs::path(std::u8string(path.begin(), path.end()));
}
std::string ToGbPath(const fs::path& path) {
  const std::u8string utf8_path = path.generic_u8string();
  return NormalizePath(std::string(utf8_path.begin(), utf8_path.end()));
}

std::unique_ptr<FileProtocol> LocalFileProtocolFactory(
    const CommonProtocolTestInit& init) {
  auto protocol = LocalFileProtocol::CreateTemp("gbtest");
  if (protocol == nullptr) {
    return nullptr;
  }
  return init.DefaultInit(std::move(protocol));
}

fs::path GetUnusedTempPath() {
  std::error_code error;
  fs::path temp_path = fs::temp_directory_path(error);
  if (error || temp_path.empty()) {
    return {};
  }
  while (true) {
    fs::path new_path =
        temp_path / absl::StrCat("gb-", GenerateAlphaTestString(10));
    if (!fs::exists(new_path, error)) {
      return new_path;
    }
  }
}

bool WriteFile(fs::path file_path, std::string_view contents) {
  std::fstream file(file_path, std::fstream::binary | std::fstream::out |
                                   std::fstream::trunc);
  if (!file.good()) {
    return false;
  }
  file << contents;
  return true;
}

bool AddContents(fs::path directory) {
  std::error_code error;
  if (!WriteFile(directory / "file-1", "1234567890")) {
    return false;
  }
  WriteFile(directory / "file-2", "abcdefghij");
  if (fs::create_directory(directory / "folder-1", error)) {
    WriteFile(directory / "folder-1/file-3", "testing");
    WriteFile(directory / "folder-1/file-4", "1,2,3");
  }
  return true;
}

bool IsDirectoryEmpty(fs::path directory) {
  std::error_code error;
  return fs::directory_iterator(directory, error) == fs::directory_iterator();
}

INSTANTIATE_TEST_SUITE_P(LocalFileProtocolTest, CommonProtocolTest,
                         Values(LocalFileProtocolFactory));

TEST(LocalFileProtocolTest, InvalidContext) {
  EXPECT_EQ(LocalFileProtocol::Create(Context()), nullptr);
}

TEST(LocalFileProtocolTest, InvalidRoot) {
  std::error_code error;
  fs::path root_path = GetUnusedTempPath();
  ASSERT_TRUE(fs::create_directory(root_path, error)) << error.message();
  ScopedCall scoped([&] { fs::remove_all(root_path, error); });

  fs::path file_path = root_path / "file";
  std::fstream file(file_path, std::fstream::binary | std::fstream::out |
                                   std::fstream::trunc);
  ASSERT_TRUE(file.good());
  file << "This file cannot be the root of a LocalFileProtocol!";
  file.close();

  Context context = ContextBuilder()
                        .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                               ToGbPath(file_path))
                        .Build();
  EXPECT_EQ(LocalFileProtocol::Create(std::move(context)), nullptr);

  context = ContextBuilder()
                .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                       ToGbPath(file_path / "subfile"))
                .Build();
  EXPECT_EQ(LocalFileProtocol::Create(std::move(context)), nullptr);

  context = ContextBuilder()
                .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                       ToGbPath(root_path / "missing/parent"))
                .Build();
  EXPECT_EQ(LocalFileProtocol::Create(std::move(context)), nullptr);
}

TEST(LocalFileProtocolTest, RelativeRoot) {
  std::error_code error;
  fs::path original_path = fs::current_path(error);
  ASSERT_FALSE(error) << error.message();
  fs::path root_path = GetUnusedTempPath();
  ASSERT_TRUE(fs::create_directory(root_path, error)) << error.message();

  // The current directory is restored first, as Windows cannot remove a
  // directory that is the current directory of a process.
  ScopedCall scoped([&] {
    fs::current_path(original_path, error);
    fs::remove_all(root_path, error);
  });

  fs::path subdir_path = root_path / "subdir";
  ASSERT_TRUE(fs::create_directory(subdir_path, error)) << error.message();

  fs::current_path(subdir_path, error);
  ASSERT_FALSE(error) << error.message();

  Context context = ContextBuilder()
                        .SetValue<std::string>(LocalFileProtocol::kKeyRoot, "")
                        .Build();
  auto protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_EQ(protocol->GetRoot(), ToGbPath(subdir_path));

  context = ContextBuilder()
                .SetValue<std::string>(LocalFileProtocol::kKeyRoot, ".")
                .Build();
  protocol = LocalFileProtocol::Create(std::move(context));
  ASSERT_NE(protocol, nullptr);
  EXPECT_EQ(protocol->GetRoot(), ToGbPath(subdir_path));

  context = ContextBuilder()
                .SetValue<std::string>(LocalFileProtocol::kKeyRoot, "..")
                .Build();
  protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_EQ(protocol->GetRoot(), ToGbPath(root_path));

  context =
      ContextBuilder()
          .SetValue<std::string>(LocalFileProtocol::kKeyRoot, "new-folder")
          .Build();
  protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_EQ(protocol->GetRoot(), ToGbPath(subdir_path / "new-folder"));
  EXPECT_TRUE(fs::is_directory(subdir_path / "new-folder", error))
      << error.message();
}

TEST(LocalFileProtocolTest, UniqueRoot) {
  std::error_code error;
  fs::path root_path = GetUnusedTempPath();
  ASSERT_TRUE(fs::create_directory(root_path, error)) << error.message();
  ScopedCall scoped([&] { fs::remove_all(root_path, error); });

  std::string root = ToGbPath(root_path);
  Context context =
      ContextBuilder()
          .SetValue<std::string>(LocalFileProtocol::kKeyRoot, root)
          .SetValue<bool>(LocalFileProtocol::kKeyUniqueRoot, true)
          .Build();
  auto protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_NE(protocol->GetRoot(), root);
  EXPECT_EQ(RemoveFilename(protocol->GetRoot()), root);

  context = ContextBuilder()
                .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                       JoinPath(root, "prefix"))
                .SetValue<bool>(LocalFileProtocol::kKeyUniqueRoot, true)
                .Build();
  protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_NE(protocol->GetRoot(), root);
  EXPECT_TRUE(absl::StartsWith(RemoveFolder(protocol->GetRoot()), "prefix_"));
  EXPECT_EQ(RemoveFilename(protocol->GetRoot()), root);
}

TEST(LocalFileProtocolTest, DeleteAtExit) {
  std::error_code error;
  fs::path root_path = GetUnusedTempPath();
  ASSERT_TRUE(fs::create_directory(root_path, error)) << error.message();
  ScopedCall scoped([&] { fs::remove_all(root_path, error); });

  std::string root = ToGbPath(root_path);
  Context context =
      ContextBuilder()
          .SetValue<std::string>(LocalFileProtocol::kKeyRoot, root)
          .SetValue<bool>(LocalFileProtocol::kKeyDeleteAtExit, true)
          .Build();
  auto protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_EQ(protocol->GetRoot(), root);
  EXPECT_TRUE(AddContents(root_path));
  EXPECT_FALSE(IsDirectoryEmpty(root_path));
  protocol.reset();
  EXPECT_TRUE(IsDirectoryEmpty(root_path));

  std::string sub_dir = JoinPath(root, "sub_dir");
  context = ContextBuilder()
                .SetValue<std::string>(LocalFileProtocol::kKeyRoot, sub_dir)
                .SetValue<bool>(LocalFileProtocol::kKeyDeleteAtExit, true)
                .Build();
  protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_EQ(protocol->GetRoot(), sub_dir);
  EXPECT_TRUE(AddContents(sub_dir));
  EXPECT_FALSE(IsDirectoryEmpty(sub_dir));
  protocol.reset();
  EXPECT_TRUE(fs::is_directory(sub_dir, error));
  EXPECT_TRUE(IsDirectoryEmpty(sub_dir));

  context = ContextBuilder()
                .SetValue<std::string>(LocalFileProtocol::kKeyRoot, sub_dir)
                .SetValue<bool>(LocalFileProtocol::kKeyUniqueRoot, true)
                .SetValue<bool>(LocalFileProtocol::kKeyDeleteAtExit, true)
                .Build();
  protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_NE(protocol->GetRoot(), sub_dir);
  std::string unique_dir = protocol->GetRoot();
  EXPECT_TRUE(AddContents(unique_dir));
  EXPECT_FALSE(IsDirectoryEmpty(unique_dir));
  protocol.reset();
  EXPECT_FALSE(fs::is_directory(unique_dir, error));
}

TEST(LocalFileProtocolTest, RootIsNormalized) {
  std::error_code error;
  fs::path root_path = GetUnusedTempPath();
  ASSERT_TRUE(fs::create_directory(root_path, error)) << error.message();
  ScopedCall scoped([&] { fs::remove_all(root_path, error); });

  const std::string root = ToGbPath(root_path);
  auto protocol = LocalFileProtocol::Create(
      ContextBuilder()
          .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                 absl::StrCat(root, "/./sub/"))
          .Build());
  ASSERT_NE(protocol, nullptr);
  EXPECT_EQ(protocol->GetRoot(), JoinPath(root, "sub"));

  // A native Windows path has separators and a drive letter to normalize.
  auto native_protocol = LocalFileProtocol::Create(
      ContextBuilder()
          .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                 (root_path / "native").string())
          .Build());
  ASSERT_NE(native_protocol, nullptr);
  EXPECT_EQ(native_protocol->GetRoot(), JoinPath(root, "native"));

  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::move(protocol)));
  ASSERT_TRUE(file_system.WriteFile("file:/file", "12345"));
  EXPECT_THAT(file_system.List("file:/"), ElementsAre("file:/file"));
}

TEST(LocalFileProtocolTest, PathsStayUnderRoot) {
  std::error_code error;
  fs::path parent_path = GetUnusedTempPath();
  ASSERT_TRUE(fs::create_directory(parent_path, error)) << error.message();
  ScopedCall scoped([&] { fs::remove_all(parent_path, error); });
  ASSERT_TRUE(fs::create_directories(parent_path / "root/host", error))
      << error.message();
  ASSERT_TRUE(WriteFile(parent_path / "root/host/file", "inside"));
  ASSERT_TRUE(WriteFile(parent_path / "outside", "outside"));

  auto protocol = LocalFileProtocol::Create(
      ContextBuilder()
          .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                 ToGbPath(parent_path / "root"))
          .Build());
  ASSERT_NE(protocol, nullptr);
  for (std::string_view path :
       {"/../outside", "/..\\outside", "/host/../../outside", "//host/file"}) {
    EXPECT_EQ(protocol->GetPathInfo("file", path).type, PathType::kInvalid)
        << path;
    EXPECT_EQ(protocol->OpenFile("file", path, kReadFileFlags), nullptr)
        << path;
    EXPECT_FALSE(protocol->DeleteFile("file", path)) << path;
  }
  EXPECT_TRUE(fs::is_regular_file(parent_path / "outside", error));
  EXPECT_TRUE(fs::is_regular_file(parent_path / "root/host/file", error));
}

TEST(LocalFileProtocolTest, CopyFileOntoItselfByAnotherName) {
  auto protocol = LocalFileProtocol::CreateTemp("gbtest");
  ASSERT_NE(protocol, nullptr);
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::move(protocol)));
  ASSERT_TRUE(file_system.WriteFile("file:/file", "12345"));

  // Windows paths ignore case.
  EXPECT_TRUE(file_system.CopyFile("file:/file", "file:/FILE"));
  std::string contents;
  ASSERT_TRUE(file_system.ReadFile("file:/file", &contents));
  EXPECT_EQ(contents, "12345");
}

TEST(LocalFileProtocolTest, RecursiveDeleteFolderFailure) {
  auto protocol = LocalFileProtocol::CreateTemp("gbtest");
  ASSERT_NE(protocol, nullptr);
  const fs::path root_path = ToFsPath(protocol->GetRoot());
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::move(protocol)));
  ASSERT_TRUE(file_system.CreateFolder("file:/folder"));
  ASSERT_TRUE(file_system.WriteFile("file:/folder/file", "12345"));

  // Windows can't delete a file that is open.
  std::fstream open_file(root_path / "folder/file", std::fstream::in);
  ASSERT_TRUE(open_file.is_open());
  EXPECT_FALSE(
      file_system.DeleteFolder("file:/folder", FolderMode::kRecursive));
  open_file.close();
  EXPECT_TRUE(file_system.DeleteFolder("file:/folder", FolderMode::kRecursive));
  EXPECT_FALSE(file_system.IsValidPath("file:/folder"));
}

TEST(LocalFileProtocolTest, CurrentFolder) {
  std::error_code error;
  fs::path original_path = fs::current_path(error);
  ASSERT_FALSE(error) << error.message();
  fs::path parent_path = GetUnusedTempPath();
  ASSERT_TRUE(fs::create_directory(parent_path, error)) << error.message();

  // The current directory is restored first, as Windows cannot remove a
  // directory that is the current directory of a process.
  ScopedCall scoped([&] {
    fs::current_path(original_path, error);
    fs::remove_all(parent_path, error);
  });

  ASSERT_TRUE(fs::create_directory(parent_path / "root", error))
      << error.message();
  ASSERT_TRUE(fs::create_directory(parent_path / "root-sibling", error))
      << error.message();
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(LocalFileProtocol::Create(
      ContextBuilder()
          .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                 ToGbPath(parent_path / "root"))
          .Build())));
  ASSERT_TRUE(file_system.CreateFolder("file:/folder"));

  EXPECT_TRUE(file_system.SetCurrentFolder("file:/folder"));
  EXPECT_EQ(file_system.GetCurrentFolder("file"), "/folder");
  EXPECT_TRUE(file_system.SetCurrentFolder("file:/"));
  EXPECT_EQ(file_system.GetCurrentFolder("file"), "/");

  // A folder beside the root whose name starts with the root's name is not
  // under the root.
  fs::current_path(parent_path / "root-sibling", error);
  ASSERT_FALSE(error) << error.message();
  EXPECT_EQ(file_system.GetCurrentFolder("file"), "");
}

TEST(LocalFileProtocolTest, NonAsciiNames) {
  // U+65E5 U+672C in UTF-8, which most Windows code pages can't represent.
  static constexpr std::string_view kName = "\xE6\x97\xA5\xE6\x9C\xAC";

  auto protocol = LocalFileProtocol::CreateTemp("gbtest");
  ASSERT_NE(protocol, nullptr);
  const fs::path root_path = ToFsPath(protocol->GetRoot());
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::move(protocol)));

  // Paths are UTF-8, and name the same file in the operating system.
  const std::string folder = absl::StrCat("file:/", kName);
  const std::string file = JoinPath(folder, kName);
  ASSERT_TRUE(file_system.CreateFolder(folder));
  ASSERT_TRUE(file_system.WriteFile(file, "12345"));
  std::error_code error;
  EXPECT_TRUE(fs::is_regular_file(root_path / ToFsPath(kName) / ToFsPath(kName),
                                  error));
  EXPECT_EQ(file_system.GetPathInfo(file).size, 5);

  // A path that isn't UTF-8 is invalid.
  EXPECT_FALSE(file_system.CreateFolder("file:/\xFF"));
  EXPECT_FALSE(file_system.WriteFile("file:/\xFF", "12345"));

  // Names from the operating system are listed as UTF-8.
  const std::string other_name = absl::StrCat(kName, "-2");
  ASSERT_TRUE(WriteFile(root_path / ToFsPath(other_name), "2"));
  EXPECT_THAT(
      file_system.List("file:/", FolderMode::kRecursive),
      UnorderedElementsAre(folder, file, absl::StrCat("file:/", other_name)));
}

}  // namespace
}  // namespace gb
