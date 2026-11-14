// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "gb/file/file_system.h"
#include "gb/file/local_file_protocol.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

namespace fs = std::filesystem;

using ::testing::ElementsAre;

// A byte that can't start a UTF-8 character, so a name holding it isn't valid
// where paths are taken as UTF-8.
fs::path GetInvalidName() { return fs::path(std::string(1, '\xFF')); }

fs::path ToFsPath(std::string_view path) {
  return fs::path(std::u8string(path.begin(), path.end()));
}

TEST(GenLocalFilePathTest, CreateWithInvalidRootPath) {
  std::error_code error;
  const fs::path temp_path = fs::temp_directory_path(error);
  ASSERT_FALSE(error) << error.message();

  // This fails, rather than rooting the protocol in the current directory.
  EXPECT_EQ(LocalFileProtocol::Create(temp_path / GetInvalidName()), nullptr);
}

TEST(GenLocalFilePathTest, ListSkipsInvalidNames) {
  auto protocol = LocalFileProtocol::CreateTemp("gbtest");
  ASSERT_NE(protocol, nullptr);
  const fs::path root_path = ToFsPath(protocol->GetRoot());
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(std::move(protocol)));
  ASSERT_TRUE(file_system.WriteFile("file:/file", "12345"));

  // A name that can't be converted to UTF-8 can't be named by a path either.
  std::fstream invalid_file(root_path / GetInvalidName(), std::fstream::out);
  ASSERT_TRUE(invalid_file.is_open());
  invalid_file.close();
  EXPECT_THAT(file_system.List("file:/"), ElementsAre("file:/file"));
}

}  // namespace
}  // namespace gb
