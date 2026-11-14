// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <filesystem>
#include <fstream>
#include <string>

#include "gb/file/file_system.h"
#include "gb/file/local_file_protocol.h"
#include "gb/file/local_path_encoding.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// These tests use no Windows headers, whose macros rename FileSystem methods
// such as MoveFile and CopyFile.

namespace gb {
namespace {

namespace fs = std::filesystem;

//------------------------------------------------------------------------------
// Windows file system behavior: names ignore case, and open files can't be
// moved or deleted.
//------------------------------------------------------------------------------

TEST(WinLocalFileSystemTest, NamesIgnoreCase) {
  auto protocol = LocalFileProtocol::CreateTemp("gbtest");
  ASSERT_NE(protocol, nullptr);
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::move(protocol)));
  ASSERT_TRUE(file_system.WriteFile("file:/file", "12345"));
  ASSERT_TRUE(file_system.WriteFile("file:/other", "abcde"));
  ASSERT_TRUE(file_system.CreateFolder("file:/folder"));

  // Another file's name in any case is taken, and a file's own name in another
  // case is the same file.
  EXPECT_FALSE(file_system.MoveFile("file:/file", "file:/OTHER"));
  EXPECT_FALSE(file_system.MoveFolder("file:/folder", "file:/OTHER"));
  EXPECT_TRUE(file_system.CopyFile("file:/file", "file:/FILE"));
  std::string contents;
  ASSERT_TRUE(file_system.ReadFile("file:/file", &contents));
  EXPECT_EQ(contents, "12345");
}

TEST(WinLocalFileSystemTest, MoveOpenFile) {
  auto protocol = LocalFileProtocol::CreateTemp("gbtest");
  ASSERT_NE(protocol, nullptr);
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::move(protocol)));
  ASSERT_TRUE(file_system.CreateFolder("file:/folder"));

  // Windows can't move a file that is open, nor the folder it is in.
  auto file = file_system.OpenFile("file:/folder/file", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  EXPECT_FALSE(file_system.MoveFile("file:/folder/file", "file:/moved"));
  EXPECT_FALSE(file_system.MoveFolder("file:/folder", "file:/moved"));
  EXPECT_FALSE(file_system.IsValidPath("file:/moved"));
  file.reset();
  EXPECT_TRUE(file_system.MoveFolder("file:/folder", "file:/moved"));
  EXPECT_TRUE(file_system.IsValidFile("file:/moved/file"));
}

TEST(WinLocalFileSystemTest, RecursiveDeleteFolderFailure) {
  auto protocol = LocalFileProtocol::CreateTemp("gbtest");
  ASSERT_NE(protocol, nullptr);
  const fs::path root_path = internal::Utf8ToLocalPath(protocol->GetRoot());
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

}  // namespace
}  // namespace gb
