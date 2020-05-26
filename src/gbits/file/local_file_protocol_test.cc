#include "gbits/file/local_file_protocol.h"

#include <filesystem>
#include <fstream>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "gbits/base/context_builder.h"
#include "gbits/base/scoped_call.h"
#include "gbits/file/common_protocol_test.h"
#include "gbits/file/file_system.h"
#include "gbits/file/path.h"
#include "gbits/test/test_util.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

namespace fs = std::filesystem;

using ::testing::Values;

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
        temp_path / absl::StrCat("gbits-", GenerateAlphaTestString(10));
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

  Context context =
      ContextBuilder()
          .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                 NormalizePath(file_path.generic_u8string()))
          .Build();
  EXPECT_EQ(LocalFileProtocol::Create(std::move(context)), nullptr);

  context = ContextBuilder()
                .SetValue<std::string>(
                    LocalFileProtocol::kKeyRoot,
                    NormalizePath((file_path / "subfile").generic_u8string()))
                .Build();
  EXPECT_EQ(LocalFileProtocol::Create(std::move(context)), nullptr);

  context =
      ContextBuilder()
          .SetValue<std::string>(
              LocalFileProtocol::kKeyRoot,
              NormalizePath((root_path / "missing/parent").generic_u8string()))
          .Build();
  EXPECT_EQ(LocalFileProtocol::Create(std::move(context)), nullptr);
}

TEST(LocalFileProtocolTest, RelativeRoot) {
  std::error_code error;
  fs::path root_path = GetUnusedTempPath();
  ASSERT_TRUE(fs::create_directory(root_path, error)) << error.message();
  ScopedCall scoped([&] { fs::remove_all(root_path, error); });

  fs::path subdir_path = root_path / "subdir";
  ASSERT_TRUE(fs::create_directory(subdir_path, error)) << error.message();

  fs::current_path(subdir_path, error);
  ASSERT_FALSE(error) << error.message();

  Context context = ContextBuilder()
                        .SetValue<std::string>(LocalFileProtocol::kKeyRoot, "")
                        .Build();
  auto protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_EQ(protocol->GetRoot(), NormalizePath(subdir_path.generic_u8string()));

  context = ContextBuilder()
                .SetValue<std::string>(LocalFileProtocol::kKeyRoot, "..")
                .Build();
  protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_EQ(protocol->GetRoot(), NormalizePath(root_path.generic_u8string()));

  context =
      ContextBuilder()
          .SetValue<std::string>(LocalFileProtocol::kKeyRoot, "new-folder")
          .Build();
  protocol = LocalFileProtocol::Create(std::move(context));
  EXPECT_EQ(protocol->GetRoot(),
            NormalizePath((subdir_path / "new-folder").generic_u8string()));
  EXPECT_TRUE(fs::is_directory(subdir_path / "new-folder", error))
      << error.message();
}

TEST(LocalFileProtocolTest, UniqueRoot) {
  std::error_code error;
  fs::path root_path = GetUnusedTempPath();
  ASSERT_TRUE(fs::create_directory(root_path, error)) << error.message();
  ScopedCall scoped([&] { fs::remove_all(root_path, error); });

  std::string root = NormalizePath(root_path.generic_u8string());
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

  std::string root = NormalizePath(root_path.generic_u8string());
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

}  // namespace
}  // namespace gb
