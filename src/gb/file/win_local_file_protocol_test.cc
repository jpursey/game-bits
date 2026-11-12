// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "gb/base/unicode.h"
#include "gb/base/win_platform.h"
#include "gb/file/file_system.h"
#include "gb/file/local_file_protocol.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>

namespace gb {
namespace {

namespace fs = std::filesystem;

using ::testing::IsEmpty;

constexpr int kFileSize = 100000;
constexpr int kLockedOffset = kFileSize / 2;
constexpr int kLockedSize = 1000;

// Another handle locking part of a file makes reading or writing that part
// fail, without any special privileges.
class WinLocalFileProtocolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto protocol = LocalFileProtocol::CreateTemp("gbtest");
    ASSERT_NE(protocol, nullptr);
    const std::string root = protocol->GetRoot();
    ASSERT_TRUE(file_system_.Register(std::move(protocol)));
    ASSERT_TRUE(
        file_system_.WriteFile("file:/file", std::string(kFileSize, 'x')));

    const fs::path file_path =
        fs::path(std::u8string(root.begin(), root.end())) / "file";
    handle_ = ::CreateFileW(file_path.c_str(), GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, 0, nullptr);
    ASSERT_NE(handle_, INVALID_HANDLE_VALUE) << GetWindowsError();
    locked_range_.Offset = kLockedOffset;
    ASSERT_TRUE(::LockFileEx(handle_, LOCKFILE_EXCLUSIVE_LOCK, 0, kLockedSize,
                             0, &locked_range_))
        << GetWindowsError();
  }

  void TearDown() override {
    if (handle_ != INVALID_HANDLE_VALUE) {
      ::CloseHandle(handle_);
    }
  }

  void Unlock() {
    ASSERT_TRUE(::UnlockFileEx(handle_, 0, kLockedSize, 0, &locked_range_))
        << GetWindowsError();
  }

  FileSystem file_system_;
  HANDLE handle_ = INVALID_HANDLE_VALUE;
  OVERLAPPED locked_range_ = {};
};

TEST_F(WinLocalFileProtocolTest, ReadFails) {
  // A read before the locked part succeeds, and one into it fails.
  auto file = file_system_.OpenFile("file:/file", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->ReadString(kLockedOffset / 2).size(), kLockedOffset / 2);
  EXPECT_TRUE(file->IsValid());
  EXPECT_THAT(file->ReadString(kLockedOffset), IsEmpty());
  EXPECT_FALSE(file->IsValid());
  file.reset();

  std::string contents;
  EXPECT_FALSE(file_system_.ReadFile("file:/file", &contents));
  EXPECT_THAT(contents, IsEmpty());

  Unlock();
  EXPECT_TRUE(file_system_.ReadFile("file:/file", &contents));
  EXPECT_EQ(contents.size(), kFileSize);
}

TEST_F(WinLocalFileProtocolTest, WriteFails) {
  // A small write is buffered, so it only fails when it reaches the file.
  auto file = file_system_.OpenFile("file:/file", kWriteFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_EQ(file->SeekTo(kLockedOffset), kLockedOffset);
  EXPECT_EQ(file->WriteString("y"), 1);
  EXPECT_FALSE(file->Close());

  // A write too big to buffer fails as it is written.
  file = file_system_.OpenFile("file:/file", kWriteFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_EQ(file->SeekTo(kLockedOffset / 2), kLockedOffset / 2);
  const std::string text(kLockedOffset, 'y');
  EXPECT_LT(file->WriteString(text), kLockedOffset);
  EXPECT_FALSE(file->IsValid());
  file.reset();

  Unlock();
  file = file_system_.OpenFile("file:/file", kWriteFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_EQ(file->SeekTo(kLockedOffset), kLockedOffset);
  EXPECT_EQ(file->WriteString(text), kLockedOffset);
  EXPECT_TRUE(file->Close());
}

//------------------------------------------------------------------------------
// Attributes are checked against what Windows itself reports.
//------------------------------------------------------------------------------

class WinLocalFileAttributesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto protocol = LocalFileProtocol::CreateTemp("gbtest");
    ASSERT_NE(protocol, nullptr);
    const std::string root = protocol->GetRoot();
    root_path_ = fs::path(std::u8string(root.begin(), root.end()));
    ASSERT_TRUE(file_system_.Register(std::move(protocol)));
  }

  // Returns the Windows attributes of `path` under the root, of the path
  // itself rather than what it links to.
  DWORD GetWindowsAttributes(std::string_view path) {
    return ::GetFileAttributesW(GetFullPath(path).c_str());
  }

  void AddWindowsAttributes(std::string_view path, DWORD attributes) {
    const fs::path full_path = GetFullPath(path);
    const DWORD old_attributes = ::GetFileAttributesW(full_path.c_str());
    ASSERT_NE(old_attributes, INVALID_FILE_ATTRIBUTES) << GetWindowsError();
    ASSERT_TRUE(
        ::SetFileAttributesW(full_path.c_str(), old_attributes | attributes))
        << GetWindowsError();
  }

  fs::path GetFullPath(std::string_view path) {
    return root_path_ / fs::path(std::u8string(path.begin(), path.end()));
  }

  // Creates a junction (a link to a folder, which needs no special privileges)
  // at `path` under the root, to `target_path` under the root.
  void CreateJunction(std::string_view path, std::string_view target_path);

  FileSystem file_system_;
  fs::path root_path_;
};

void WinLocalFileAttributesTest::CreateJunction(std::string_view path,
                                                std::string_view target_path) {
  const fs::path full_path = GetFullPath(path);
  std::error_code error;
  ASSERT_TRUE(fs::create_directory(full_path, error)) << error.message();
  const std::wstring target =
      GetFullPath(target_path).make_preferred().wstring();
  const std::wstring substitute_name = L"\\??\\" + target;

  // The layout of REPARSE_DATA_BUFFER's MountPointReparseBuffer, which only
  // the Windows driver headers define: both names, each null terminated,
  // follow the header.
  struct Header {
    DWORD tag;
    WORD data_size;
    WORD reserved;
    WORD substitute_name_offset;
    WORD substitute_name_size;
    WORD print_name_offset;
    WORD print_name_size;
  };
  const WORD substitute_name_size =
      static_cast<WORD>(substitute_name.size() * sizeof(wchar_t));
  const WORD print_name_size =
      static_cast<WORD>(target.size() * sizeof(wchar_t));
  const WORD names_size =
      substitute_name_size + print_name_size + 2 * sizeof(wchar_t);
  std::vector<char> buffer(sizeof(Header) + names_size);
  Header header = {};
  header.tag = IO_REPARSE_TAG_MOUNT_POINT;
  header.data_size = static_cast<WORD>(sizeof(Header) - 2 * sizeof(WORD) -
                                       sizeof(DWORD) + names_size);
  header.substitute_name_size = substitute_name_size;
  header.print_name_offset = substitute_name_size + sizeof(wchar_t);
  header.print_name_size = print_name_size;
  std::memcpy(buffer.data(), &header, sizeof(header));
  std::memcpy(buffer.data() + sizeof(Header), substitute_name.c_str(),
              substitute_name_size + sizeof(wchar_t));
  std::memcpy(buffer.data() + sizeof(Header) + header.print_name_offset,
              target.c_str(), print_name_size + sizeof(wchar_t));

  const HANDLE handle = ::CreateFileW(
      full_path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  ASSERT_NE(handle, INVALID_HANDLE_VALUE) << GetWindowsError();
  DWORD bytes = 0;
  const bool success = ::DeviceIoControl(
      handle, FSCTL_SET_REPARSE_POINT, buffer.data(),
      static_cast<DWORD>(buffer.size()), nullptr, 0, &bytes, nullptr);
  const std::string windows_error = success ? "" : GetWindowsError();
  ::CloseHandle(handle);
  ASSERT_TRUE(success) << windows_error;
}

TEST_F(WinLocalFileAttributesTest, OtherAttributesAreKept) {
  ASSERT_TRUE(file_system_.WriteFile("file:/file", "file"));
  ASSERT_TRUE(file_system_.CreateFolder("file:/folder"));
  constexpr DWORD kOtherAttributes =
      FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
  for (std::string_view path : {"file", "folder"}) {
    SCOPED_TRACE(path);
    const std::string gb_path = absl::StrCat("file:/", path);
    AddWindowsAttributes(path, kOtherAttributes);
    const DWORD windows_attributes = GetWindowsAttributes(path);
    EXPECT_TRUE(file_system_.GetPathInfo(gb_path).attributes.IsEmpty());

    EXPECT_TRUE(file_system_.SetAttributes(
        gb_path, {FileAttribute::kReadOnly, FileAttribute::kHidden}));
    EXPECT_EQ(GetWindowsAttributes(path), windows_attributes |
                                              FILE_ATTRIBUTE_READONLY |
                                              FILE_ATTRIBUTE_HIDDEN);
    EXPECT_TRUE(file_system_.SetAttributes(gb_path, {}));
    EXPECT_EQ(GetWindowsAttributes(path), windows_attributes);
  }

  // With no attributes left at all, a file has none.
  ASSERT_TRUE(
      ::SetFileAttributesW(GetFullPath("file").c_str(), FILE_ATTRIBUTE_NORMAL))
      << GetWindowsError();
  EXPECT_TRUE(file_system_.SetAttributes("file:/file", FileAttribute::kHidden));
  EXPECT_EQ(GetWindowsAttributes("file"), FILE_ATTRIBUTE_HIDDEN);
  EXPECT_TRUE(file_system_.SetAttributes("file:/file", {}));
  EXPECT_EQ(GetWindowsAttributes("file"), FILE_ATTRIBUTE_NORMAL);
}

TEST_F(WinLocalFileAttributesTest, LinkHasTargetAttributes) {
  ASSERT_TRUE(file_system_.CreateFolder("file:/target"));
  ASSERT_TRUE(
      file_system_.SetAttributes("file:/target", FileAttribute::kReadOnly));
  ASSERT_NO_FATAL_FAILURE(CreateJunction("link", "target"));
  const DWORD link_attributes = GetWindowsAttributes("link");
  ASSERT_NE(link_attributes & FILE_ATTRIBUTE_REPARSE_POINT, 0);
  EXPECT_EQ(link_attributes & FILE_ATTRIBUTE_READONLY, 0);

  EXPECT_EQ(file_system_.GetPathInfo("file:/link").attributes,
            FileAttributes(FileAttribute::kReadOnly));
  EXPECT_TRUE(file_system_.SetAttributes("file:/link", FileAttribute::kHidden));
  EXPECT_EQ(file_system_.GetPathInfo("file:/target").attributes,
            FileAttributes(FileAttribute::kHidden));
  EXPECT_EQ(GetWindowsAttributes("link"), link_attributes);
}

TEST(WinLocalFileProtocolOsRootTest, DriveFolders) {
  std::error_code error;
  const fs::path temp_path = fs::temp_directory_path(error);
  ASSERT_FALSE(error) << error.message();
  const std::string root_name = ToUtf8(temp_path.root_name().u16string());
  ASSERT_EQ(root_name.size(), 2) << root_name;
  const std::string drive =
      absl::StrCat("/", absl::AsciiStrToLower(root_name.substr(0, 1)));

  // A drive is a folder, but the root that would hold the drives isn't, nor is
  // a first folder that isn't a drive letter.
  auto protocol = LocalFileProtocol::CreateOsRoot();
  EXPECT_EQ(protocol->GetRoot(), "");
  EXPECT_EQ(protocol->GetPathInfo("file", drive).type, PathType::kFolder);
  EXPECT_EQ(protocol->GetPathInfo("file", "/").type, PathType::kInvalid);

  // A drive's root folder is normally hidden on Windows, but as a root folder
  // has no attributes.
  EXPECT_TRUE(protocol->GetPathInfo("file", drive).attributes.IsEmpty());
  EXPECT_THAT(
      protocol->List("file", "/", {}, FolderMode::kNormal, kAllPathTypes),
      IsEmpty());
  EXPECT_EQ(protocol->GetPathInfo("file", "/cc").type, PathType::kInvalid);
  EXPECT_EQ(protocol->GetPathInfo("file", absl::AsciiStrToUpper(drive)).type,
            PathType::kInvalid);
}

}  // namespace
}  // namespace gb
