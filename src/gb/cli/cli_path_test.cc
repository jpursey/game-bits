// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_path.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "gb/base/context.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_command_line.h"
#include "gb/file/file_system.h"
#include "gb/file/file_types.h"
#include "gb/file/memory_file_protocol.h"
#include "gb/file/path.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::StartsWith;

// A memory protocol whose current folder is "/data".
class CurrentFolderProtocol : public MemoryFileProtocol {
 public:
  CurrentFolderProtocol()
      : MemoryFileProtocol(kReadWriteFileProtocolFlags +
                           FileProtocolFlag::kCurrentPath) {}

 protected:
  std::string DoGetCurrentPath(std::string_view) override { return "/data"; }
};

// Holds a file system with memory protocols registered as "file", which is the
// default protocol and has a current folder, "mem", and "c", which stands in
// for a drive (see LocalFileProtocol::CreateOsRoot()). The context holds the
// file system, and the fixture keeps the errors ValidatedContext reports.
class CliPathTest : public ::testing::Test {
 protected:
  CliPathTest() {
    EXPECT_TRUE(file_system_.Register(std::make_unique<CurrentFolderProtocol>(),
                                      "file"));
    EXPECT_TRUE(
        file_system_.Register(std::make_unique<MemoryFileProtocol>(), "mem"));
    EXPECT_TRUE(
        file_system_.Register(std::make_unique<MemoryFileProtocol>(), "c"));
    EXPECT_TRUE(file_system_.SetDefaultProtocol("file"));
    context_.SetPtr<FileSystem>(&file_system_);
    AddFile("/data/input.txt");
    AddFile("c:/src/input.txt");
    AddFile("mem:/mem_only.txt");
    ValidatedContext::SetGlobalErrorCallback(
        [this](const std::string& message) {
          context_errors_.push_back(message);
        });
  }
  ~CliPathTest() override { ValidatedContext::SetGlobalErrorCallback(nullptr); }

  // Adds a file, and the folders above it.
  void AddFile(std::string_view path) {
    EXPECT_TRUE(file_system_.CreateFolder(RemoveFilename(path),
                                          FolderMode::kRecursive));
    EXPECT_TRUE(file_system_.WriteFile(path, ""));
  }

  absl::Status Validate(const CliValidator<std::string_view>& validator,
                        std::string_view path) {
    ValidatedContext context(
        &context_, {kConstraintCliFileSystem, kConstraintCliFileProtocol});
    return validator.validate(context, path);
  }

  FileSystem file_system_;
  Context context_;
  std::vector<std::string> context_errors_;
};

//==============================================================================
// Validators
//==============================================================================

TEST_F(CliPathTest, PathExists) {
  EXPECT_TRUE(Validate(kCliPathExists, "/data/input.txt").ok());
  EXPECT_TRUE(Validate(kCliPathExists, "/data").ok());
  EXPECT_TRUE(Validate(kCliPathExists, "C:\\src\\input.txt").ok());
  EXPECT_TRUE(Validate(kCliPathExists, "C:\\src").ok());
  EXPECT_EQ(Validate(kCliPathExists, "/data/missing.txt").message(),
            "\"/data/missing.txt\" doesn't exist");
  EXPECT_EQ(Validate(kCliPathExists, "").message(), "\"\" doesn't exist");
}

TEST_F(CliPathTest, FileExists) {
  EXPECT_TRUE(Validate(kCliFileExists, "/data/input.txt").ok());
  EXPECT_TRUE(Validate(kCliFileExists, "C:\\src\\input.txt").ok());
  EXPECT_EQ(Validate(kCliFileExists, "/data").message(),
            "\"/data\" isn't a file");
  EXPECT_EQ(Validate(kCliFileExists, "/data/missing.txt").message(),
            "\"/data/missing.txt\" doesn't exist");
}

TEST_F(CliPathTest, FolderExists) {
  EXPECT_TRUE(Validate(kCliFolderExists, "/data").ok());
  EXPECT_TRUE(Validate(kCliFolderExists, "C:\\src").ok());
  EXPECT_EQ(Validate(kCliFolderExists, "/data/input.txt").message(),
            "\"/data/input.txt\" isn't a folder");
  EXPECT_EQ(Validate(kCliFolderExists, "/missing").message(),
            "\"/missing\" doesn't exist");
}

TEST_F(CliPathTest, FileWritable) {
  EXPECT_TRUE(Validate(kCliFileWritable, "/data/output.bin").ok());
  EXPECT_TRUE(Validate(kCliFileWritable, "/data/input.txt").ok());
  EXPECT_TRUE(Validate(kCliFileWritable, "/output.bin").ok());
  EXPECT_TRUE(Validate(kCliFileWritable, "C:\\src\\output.bin").ok());
  EXPECT_EQ(Validate(kCliFileWritable, "/missing/output.bin").message(),
            "\"/missing/output.bin\"'s folder doesn't exist");
  EXPECT_EQ(Validate(kCliFileWritable, "/data/input.txt/output.bin").message(),
            "\"/data/input.txt/output.bin\"'s folder doesn't exist");
}

TEST_F(CliPathTest, FileWritableForARelativePath) {
  EXPECT_TRUE(Validate(kCliFileWritable, "output.bin").ok());
  EXPECT_TRUE(Validate(kCliFileWritable, "../output.bin").ok());
  EXPECT_EQ(Validate(kCliFileWritable, "missing\\output.bin").message(),
            "\"missing\\output.bin\"'s folder doesn't exist");
}

TEST_F(CliPathTest, FileWritableForAFolder) {
  EXPECT_EQ(Validate(kCliFileWritable, "/data").message(),
            "\"/data\" is a folder");
  EXPECT_EQ(Validate(kCliFileWritable, "C:\\src").message(),
            "\"C:\\src\" is a folder");
}

TEST_F(CliPathTest, FileWritableWithoutAFileName) {
  for (const std::string_view path :
       {"", "/", "C:\\", "c:", ".", "..", "a/..", "/data/", "output\\"}) {
    EXPECT_EQ(Validate(kCliFileWritable, path).message(),
              absl::StrCat("\"", path, "\" has no file name"));
  }
}

TEST_F(CliPathTest, FileWritableForAnInvalidPath) {
  EXPECT_EQ(Validate(kCliFileWritable, "proto-call:/a/b").message(),
            "\"proto-call:/a/b\" isn't a path");
}

TEST_F(CliPathTest, FileWritableForAReadOnlyProtocol) {
  EXPECT_TRUE(file_system_.Register(
      std::make_unique<MemoryFileProtocol>(kReadOnlyFileProtocolFlags), "ro"));
  EXPECT_EQ(Validate(kCliFileWritable, "ro:/output.bin").message(),
            "\"ro:/output.bin\" can't be written");
  EXPECT_EQ(Validate(kCliFileWritable, "zz:/output.bin").message(),
            "\"zz:/output.bin\" can't be written");
}

TEST_F(CliPathTest, ValidatorsUseTheContextsProtocol) {
  EXPECT_FALSE(Validate(kCliFileExists, "/mem_only.txt").ok());
  context_.SetValue<std::string>(kKeyCliFileProtocol, "mem");
  EXPECT_TRUE(Validate(kCliFileExists, "/mem_only.txt").ok());
  EXPECT_FALSE(Validate(kCliFileExists, "/data/input.txt").ok());

  // A path with a protocol of its own keeps it.
  EXPECT_TRUE(Validate(kCliFileExists, "file:/data/input.txt").ok());
  EXPECT_TRUE(Validate(kCliFileExists, "C:\\src\\input.txt").ok());
  EXPECT_FALSE(Validate(kCliFileWritable, "/data/output.bin").ok());
  EXPECT_TRUE(Validate(kCliFileWritable, "file:/data/output.bin").ok());
}

TEST_F(CliPathTest, ValidatorsWithAnEmptyProtocol) {
  context_.SetValue<std::string>(kKeyCliFileProtocol, "");
  EXPECT_TRUE(Validate(kCliFileExists, "/data/input.txt").ok());
  EXPECT_FALSE(Validate(kCliFileExists, "/mem_only.txt").ok());
}

TEST_F(CliPathTest, ValidatorsWithAMissingProtocol) {
  context_.SetValue<std::string>(kKeyCliFileProtocol, "missing");
  for (const CliValidator<std::string_view>* validator :
       {&kCliPathExists, &kCliFileExists, &kCliFolderExists,
        &kCliFileWritable}) {
    const absl::Status status = Validate(*validator, "/data/input.txt");
    EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
    EXPECT_EQ(status.message(),
              "paths are opened with the \"missing\" protocol, which the file "
              "system doesn't have");
  }
}

TEST_F(CliPathTest, ValidatorsWithNoDefaultProtocol) {
  FileSystem file_system;
  EXPECT_TRUE(
      file_system.Register(std::make_unique<MemoryFileProtocol>(), "mem"));
  context_.SetPtr<FileSystem>(&file_system);
  const absl::Status status = Validate(kCliFileExists, "/data/input.txt");
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_EQ(status.message(),
            "paths are opened with the default protocol, which the file "
            "system doesn't have");

  // A protocol the context names doesn't need a default protocol.
  context_.SetValue<std::string>(kKeyCliFileProtocol, "mem");
  EXPECT_EQ(Validate(kCliFileExists, "/data/input.txt").message(),
            "\"/data/input.txt\" doesn't exist");
}

//==============================================================================
// Validators in definitions
//==============================================================================

const CliFlag<std::string_view> kOutput = {
    .name = "output",
    .validators = {&kCliFolderExists},
};
const CliArg<std::string_view> kInput = {
    .name = "input",
    .validators = {&kCliFileExists},
};
const CliCommand kProgram = {
    .name = "prog",
    .flags = {&kOutput},
    .args = {&kInput},
};

TEST_F(CliPathTest, InDefinitions) {
  const std::vector<std::string_view> args = {"--output", "/data",
                                              "/data/input.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&context_, &kProgram, args);
  EXPECT_TRUE(command_line.ok()) << command_line.status();

  const std::vector<std::string_view> missing_args = {"/data/missing.txt"};
  command_line = ParseCommandLine(&context_, &kProgram, missing_args);
  EXPECT_THAT(
      command_line.status().message(),
      StartsWith("prog: <input>: \"/data/missing.txt\" doesn't exist."));
  EXPECT_THAT(context_errors_, ElementsAre());
}

TEST_F(CliPathTest, InDefinitionsWithAMissingProtocol) {
  context_.SetValue<std::string>(kKeyCliFileProtocol, "missing");
  const std::vector<std::string_view> args = {"/data/input.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&context_, &kProgram, args);
  EXPECT_TRUE(absl::IsFailedPrecondition(command_line.status()))
      << command_line.status();
  EXPECT_EQ(command_line.status().message(),
            "prog: <input>: paths are opened with the \"missing\" protocol, "
            "which the file system doesn't have");
}

TEST_F(CliPathTest, WithoutAFileSystem) {
  Context context;
  const std::vector<std::string_view> args = {"/data/input.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&context, &kProgram, args);
  EXPECT_TRUE(absl::IsFailedPrecondition(command_line.status()))
      << command_line.status();
  EXPECT_THAT(context_errors_, ElementsAre(HasSubstr("FileSystem")));
}

}  // namespace
}  // namespace gb
