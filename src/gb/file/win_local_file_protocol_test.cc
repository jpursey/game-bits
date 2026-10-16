// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <filesystem>
#include <string>

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

}  // namespace
}  // namespace gb
