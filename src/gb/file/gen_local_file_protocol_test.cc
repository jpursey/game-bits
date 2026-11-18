// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "gb/file/file_system.h"
#include "gb/file/local_file_protocol.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::UnorderedElementsAre;

// Names Windows opens as devices are ordinary names elsewhere.
TEST(GenLocalFileProtocolTest, WindowsDeviceNamesAreFiles) {
  FileSystem file_system;
  ASSERT_TRUE(file_system.Register(LocalFileProtocol::CreateTemp("gbtest")));
  for (std::string_view name : {"con", "NUL", "com1", "aux.txt"}) {
    SCOPED_TRACE(name);
    const std::string path = absl::StrCat("file:/", name);
    EXPECT_TRUE(file_system.WriteFile(path, "file"));
    EXPECT_EQ(file_system.GetPathInfo(path).type, PathType::kFile);
  }
  ASSERT_TRUE(file_system.CreateFolder("file:/prn"));
  EXPECT_TRUE(file_system.WriteFile("file:/prn/file", "file"));
  EXPECT_THAT(
      file_system.List("file:/", FolderMode::kRecursive),
      UnorderedElementsAre("file:/con", "file:/NUL", "file:/com1",
                           "file:/aux.txt", "file:/prn", "file:/prn/file"));
}

}  // namespace
}  // namespace gb
