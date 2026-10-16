// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <filesystem>
#include <string>

#include "absl/cleanup/cleanup.h"
#include "gb/base/win_platform.h"
#include "gb/file/file_system.h"
#include "gb/file/local_file_protocol.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb {
namespace {

namespace fs = std::filesystem;

using ::testing::IsEmpty;

// Another handle locking part of a file makes reading that part fail, without
// any special privileges.
TEST(WinLocalFileProtocolTest, ReadFails) {
  static constexpr int kFileSize = 100000;
  static constexpr int kLockedOffset = kFileSize / 2;
  static constexpr int kLockedSize = 1000;

  auto protocol = LocalFileProtocol::CreateTemp("gbtest");
  ASSERT_NE(protocol, nullptr);
  const std::string root = protocol->GetRoot();
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::move(protocol)));
  ASSERT_TRUE(file_system.WriteFile("file:/file", std::string(kFileSize, 'x')));

  const fs::path file_path =
      fs::path(std::u8string(root.begin(), root.end())) / "file";
  HANDLE handle = ::CreateFileW(file_path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, 0, nullptr);
  ASSERT_NE(handle, INVALID_HANDLE_VALUE) << GetWindowsError();
  absl::Cleanup close_handle = [handle] { ::CloseHandle(handle); };
  OVERLAPPED locked_range = {};
  locked_range.Offset = kLockedOffset;
  ASSERT_TRUE(::LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK, 0, kLockedSize, 0,
                           &locked_range))
      << GetWindowsError();

  // A read before the locked part succeeds, and one into it fails.
  auto file = file_system.OpenFile("file:/file", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->ReadString(kLockedOffset / 2).size(), kLockedOffset / 2);
  EXPECT_TRUE(file->IsValid());
  EXPECT_THAT(file->ReadString(kLockedOffset), IsEmpty());
  EXPECT_FALSE(file->IsValid());
  file.reset();

  std::string contents;
  EXPECT_FALSE(file_system.ReadFile("file:/file", &contents));
  EXPECT_THAT(contents, IsEmpty());

  ASSERT_TRUE(::UnlockFileEx(handle, 0, kLockedSize, 0, &locked_range))
      << GetWindowsError();
  EXPECT_TRUE(file_system.ReadFile("file:/file", &contents));
  EXPECT_EQ(contents.size(), kFileSize);
}

}  // namespace
}  // namespace gb
