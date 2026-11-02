// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "gb/base/context_builder.h"
#include "gb/base/unicode.h"
#include "gb/file/file_system.h"
#include "gb/file/local_current_path.h"
#include "gb/file/local_file_protocol.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

namespace fs = std::filesystem;

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

// Holds a file system with a protocol from LocalFileProtocol::CreateOsRoot()
// under its default names ("file", the default protocol, and each drive), and
// a new folder in the temp folder, which is deleted after each test. The
// current folders of the process and of the temp folder's drive are restored
// after each test.
class WinLocalFileProtocolDriveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::error_code error;
    original_path_ = fs::current_path(error);
    ASSERT_FALSE(error) << error.message();
    temp_ = LocalFileProtocol::CreateTemp("gbtest");
    ASSERT_NE(temp_, nullptr);
    root_ = temp_->GetRoot();
    ASSERT_GT(root_.size(), 2);
    ASSERT_EQ(root_[1], ':') << root_;
    drive_ = root_.substr(0, 1);
    windows_drive_ = absl::AsciiStrToUpper(drive_);
    original_drive_path_ = fs::absolute(absl::StrCat(drive_, ":"), error);
    ASSERT_FALSE(error) << error.message();
    ASSERT_TRUE(file_system_.Register(LocalFileProtocol::CreateOsRoot()));
    ASSERT_TRUE(file_system_.SetDefaultProtocol("file"));
  }

  // The current folders are restored before the temp folder is deleted, as
  // Windows can't delete a process's current folder.
  void TearDown() override {
    internal::SetLocalCurrentPath(original_drive_path_);
    internal::SetLocalCurrentPath(original_path_);
  }

  // Returns true if the original current folder, where the tests are run from,
  // is on another drive than the temp folder.
  bool HasOtherDrive() const {
    const std::string other_drive =
        absl::AsciiStrToLower(original_path_.root_name().string());
    return other_drive.size() == 2 && other_drive.substr(0, 1) != drive_;
  }

  // Returns a new folder in the original current folder, which is deleted with
  // the protocol, or null if it can't be created.
  std::unique_ptr<LocalFileProtocol> CreateOtherDriveFolder() const {
    return LocalFileProtocol::Create(
        ContextBuilder()
            .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                   ToUtf8(original_path_.generic_u16string()))
            .SetValue<bool>(LocalFileProtocol::kKeyUniqueRoot, true)
            .SetValue<bool>(LocalFileProtocol::kKeyDeleteAtExit, true)
            .Build());
  }

  // Returns `relative` under the temp folder, as the operating system writes
  // it: "C:\Users\...\gbtest...\relative".
  std::string GetWindowsPath(std::string_view relative) {
    return absl::StrReplaceAll(
        absl::StrCat(windows_drive_, root_.substr(1), "/", relative),
        {{"/", "\\"}});
  }

  // Returns `relative` under the temp folder, as a native path.
  fs::path GetNativePath(std::string_view relative) {
    const std::string path = absl::StrCat(root_, "/", relative);
    return fs::path(std::u8string(path.begin(), path.end()));
  }

  fs::path original_path_;
  fs::path original_drive_path_;
  std::unique_ptr<LocalFileProtocol> temp_;
  std::string root_;
  std::string drive_;
  std::string windows_drive_;
  FileSystem file_system_;
};

TEST_F(WinLocalFileProtocolDriveTest, RegistersEveryDriveLetter) {
  EXPECT_TRUE(file_system_.IsRegistered("file"));
  for (char drive = 'a'; drive <= 'z'; ++drive) {
    EXPECT_TRUE(file_system_.IsRegistered(std::string(1, drive))) << drive;
  }

  // A protocol with its own root has no drives.
  EXPECT_THAT(temp_->GetDefaultNames(), ElementsAre("file"));
}

TEST_F(WinLocalFileProtocolDriveTest, CanonicalPaths) {
  auto protocol = LocalFileProtocol::CreateOsRoot();
  EXPECT_EQ(protocol->GetCanonicalPath("c", "/src/a"), "/c/src/a");
  EXPECT_EQ(protocol->GetCanonicalPath("c", "/"), "/c");
  EXPECT_EQ(protocol->GetCanonicalPath("file", "/c/src/a"), "/c/src/a");

  // A protocol with its own root has the same paths under any name.
  EXPECT_EQ(temp_->GetCanonicalPath("c", "/src/a"), "/src/a");
}

TEST_F(WinLocalFileProtocolDriveTest, OpensWindowsPaths) {
  ASSERT_TRUE(file_system_.WriteFile(GetWindowsPath("file.txt"), "text"));
  EXPECT_TRUE(fs::is_regular_file(GetNativePath("file.txt")));

  // The drive's protocol and the "file" protocol open the same file.
  std::string contents;
  EXPECT_TRUE(
      file_system_.ReadFile(absl::StrCat(root_, "/file.txt"), &contents));
  EXPECT_EQ(contents, "text");
  EXPECT_TRUE(file_system_.ReadFile(
      absl::StrCat("file:/", drive_, root_.substr(2), "/file.txt"), &contents));
  EXPECT_EQ(contents, "text");

  // The drive's root is a folder.
  EXPECT_EQ(file_system_.GetPathInfo(absl::StrCat(drive_, ":/")).type,
            PathType::kFolder);
}

TEST_F(WinLocalFileProtocolDriveTest, FoldersAndFiles) {
  ASSERT_TRUE(file_system_.CreateFolder(GetWindowsPath("a\\b"),
                                        FolderMode::kRecursive));
  ASSERT_TRUE(file_system_.WriteFile(GetWindowsPath("a\\b\\file.txt"), "x"));
  EXPECT_THAT(file_system_.List(GetWindowsPath("a"), FolderMode::kRecursive),
              UnorderedElementsAre(absl::StrCat(root_, "/a/b"),
                                   absl::StrCat(root_, "/a/b/file.txt")));

  EXPECT_TRUE(file_system_.CopyFile(GetWindowsPath("a\\b\\file.txt"),
                                    GetWindowsPath("a\\copy.txt")));
  EXPECT_TRUE(fs::is_regular_file(GetNativePath("a/copy.txt")));
  EXPECT_TRUE(
      file_system_.CopyFolder(GetWindowsPath("a\\b"), GetWindowsPath("c")));
  EXPECT_TRUE(fs::is_regular_file(GetNativePath("c/file.txt")));

  EXPECT_TRUE(file_system_.DeleteFile(GetWindowsPath("a\\copy.txt")));
  EXPECT_FALSE(fs::exists(GetNativePath("a/copy.txt")));
  EXPECT_TRUE(
      file_system_.DeleteFolder(GetWindowsPath("a"), FolderMode::kRecursive));
  EXPECT_FALSE(fs::exists(GetNativePath("a")));
}

TEST_F(WinLocalFileProtocolDriveTest, RelativeToTheDrivesCurrentFolder) {
  ASSERT_TRUE(file_system_.WriteFile(GetWindowsPath("file.txt"), "text"));
  std::error_code error;
  fs::current_path(GetNativePath(""), error);
  ASSERT_FALSE(error) << error.message();
  EXPECT_EQ(file_system_.GetCurrentFolder(drive_), root_.substr(2));

  std::string contents;
  EXPECT_TRUE(file_system_.ReadFile(absl::StrCat(windows_drive_, ":file.txt"),
                                    &contents));
  EXPECT_EQ(contents, "text");

  // Setting the drive's current folder sets the process's.
  ASSERT_TRUE(
      file_system_.CreateFolder(GetWindowsPath("sub"), FolderMode::kNormal));
  EXPECT_TRUE(
      file_system_.SetCurrentFolder(absl::StrCat(windows_drive_, ":sub")));
  EXPECT_EQ(fs::current_path(error), GetNativePath("sub"));
  EXPECT_EQ(file_system_.GetCurrentFolder(drive_),
            absl::StrCat(root_.substr(2), "/sub"));
}

TEST_F(WinLocalFileProtocolDriveTest, CopiesBetweenDrives) {
  if (!HasOtherDrive()) {
    GTEST_SKIP() << "The temp and current folders are on the same drive.";
  }
  auto other = CreateOtherDriveFolder();
  ASSERT_NE(other, nullptr);
  const std::string other_root = other->GetRoot();

  ASSERT_TRUE(
      file_system_.CreateFolder(GetWindowsPath("folder"), FolderMode::kNormal));
  ASSERT_TRUE(
      file_system_.WriteFile(GetWindowsPath("folder\\file.txt"), "text"));
  EXPECT_TRUE(file_system_.CopyFile(GetWindowsPath("folder\\file.txt"),
                                    absl::StrCat(other_root, "/copy.txt")));
  EXPECT_TRUE(file_system_.CopyFolder(GetWindowsPath("folder"),
                                      absl::StrCat(other_root, "/folder")));

  // Each copy is on the other drive, not at the same path on this one.
  std::string contents;
  EXPECT_TRUE(
      file_system_.ReadFile(absl::StrCat(other_root, "/copy.txt"), &contents));
  EXPECT_EQ(contents, "text");
  EXPECT_TRUE(file_system_.ReadFile(
      absl::StrCat(other_root, "/folder/file.txt"), &contents));
  EXPECT_EQ(contents, "text");
  EXPECT_FALSE(file_system_.IsValidPath(
      absl::StrCat(drive_, other_root.substr(1), "/copy.txt")));
}

TEST_F(WinLocalFileProtocolDriveTest, CurrentFolderForEachDrive) {
  if (!HasOtherDrive()) {
    GTEST_SKIP() << "The temp and current folders are on the same drive.";
  }
  auto other = CreateOtherDriveFolder();
  ASSERT_NE(other, nullptr);
  const std::string other_root = other->GetRoot();
  const std::string other_drive = other_root.substr(0, 1);
  ASSERT_TRUE(
      file_system_.CreateFolder(GetWindowsPath("foo"), FolderMode::kNormal));
  ASSERT_TRUE(
      file_system_.CreateFolder(GetWindowsPath("other"), FolderMode::kNormal));
  ASSERT_TRUE(file_system_.CreateFolder(absl::StrCat(other_root, "/bar"),
                                        FolderMode::kNormal));
  ASSERT_TRUE(file_system_.WriteFile(GetWindowsPath("foo\\file.txt"), "foo"));
  ASSERT_TRUE(
      file_system_.WriteFile(GetWindowsPath("other\\file.txt"), "other"));
  ASSERT_TRUE(
      file_system_.WriteFile(absl::StrCat(other_root, "/bar/file.txt"), "bar"));

  // Each drive keeps its own current folder, and "file" has the last one set.
  ASSERT_TRUE(file_system_.SetCurrentFolder(GetWindowsPath("foo")));
  ASSERT_TRUE(file_system_.SetCurrentFolder(absl::StrCat(other_root, "/bar")));
  std::string contents;
  EXPECT_TRUE(file_system_.ReadFile(absl::StrCat(windows_drive_, ":file.txt"),
                                    &contents));
  EXPECT_EQ(contents, "foo");
  EXPECT_TRUE(
      file_system_.ReadFile(absl::StrCat(other_drive, ":file.txt"), &contents));
  EXPECT_EQ(contents, "bar");
  EXPECT_TRUE(file_system_.ReadFile("file:file.txt", &contents));
  EXPECT_EQ(contents, "bar");

  // Setting it for "file" sets its drive's too, and leaves the other drive's.
  ASSERT_TRUE(file_system_.SetCurrentFolder(
      absl::StrCat("file:/", drive_, root_.substr(2), "/other")));
  EXPECT_TRUE(file_system_.ReadFile(absl::StrCat(windows_drive_, ":file.txt"),
                                    &contents));
  EXPECT_EQ(contents, "other");
  EXPECT_TRUE(file_system_.ReadFile("file:file.txt", &contents));
  EXPECT_EQ(contents, "other");
  EXPECT_TRUE(
      file_system_.ReadFile(absl::StrCat(other_drive, ":file.txt"), &contents));
  EXPECT_EQ(contents, "bar");
}

TEST_F(WinLocalFileProtocolDriveTest, CopiesBetweenDriveAndFile) {
  ASSERT_TRUE(
      file_system_.CreateFolder(GetWindowsPath("folder"), FolderMode::kNormal));
  ASSERT_TRUE(
      file_system_.WriteFile(GetWindowsPath("folder\\file.txt"), "text"));
  const std::string file_path = absl::StrCat("file:/", drive_, root_.substr(2));

  // A file copied onto itself by its other name keeps its contents.
  EXPECT_TRUE(
      file_system_.CopyFile(GetWindowsPath("folder\\file.txt"),
                            absl::StrCat(file_path, "/folder/file.txt")));
  std::string contents;
  EXPECT_TRUE(
      file_system_.ReadFile(GetWindowsPath("folder\\file.txt"), &contents));
  EXPECT_EQ(contents, "text");

  // A folder can't be copied into itself by its other name.
  EXPECT_FALSE(file_system_.CopyFolder(GetWindowsPath("folder"),
                                       absl::StrCat(file_path, "/folder/sub")));
  EXPECT_FALSE(fs::exists(GetNativePath("folder/sub")));

  EXPECT_TRUE(file_system_.CopyFolder(GetWindowsPath("folder"),
                                      absl::StrCat(file_path, "/copy")));
  EXPECT_TRUE(fs::is_regular_file(GetNativePath("copy/file.txt")));
}

TEST_F(WinLocalFileProtocolDriveTest, MissingDrive) {
  std::string root;
  for (char drive = 'z'; drive >= 'a'; --drive) {
    std::error_code error;
    const std::string drive_root = absl::StrCat(std::string(1, drive), ":/");
    if (!fs::exists(drive_root, error)) {
      root = drive_root;
      break;
    }
  }
  ASSERT_FALSE(root.empty()) << "every drive letter is in use";

  EXPECT_EQ(file_system_.GetPathInfo(root).type, PathType::kInvalid);
  EXPECT_FALSE(file_system_.WriteFile(absl::StrCat(root, "file.txt"), "x"));
  EXPECT_THAT(file_system_.List(root), ElementsAre());
}

}  // namespace
}  // namespace gb
