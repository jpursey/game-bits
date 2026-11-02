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
#include "gb/base/context.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_command_line.h"
#include "gb/file/file_system.h"
#include "gb/file/memory_file_protocol.h"
#include "gb/file/path.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::StartsWith;

// Holds a file system with memory protocols registered as "file", which is the
// default protocol, "mem", and "c", which stands in for a drive (see
// LocalFileProtocol::CreateOsRoot()). The context holds the file system, and
// the fixture keeps the errors ValidatedContext reports.
class CliPathTest : public ::testing::Test {
 protected:
  CliPathTest() {
    EXPECT_TRUE(
        file_system_.Register(std::make_unique<MemoryFileProtocol>(), "file"));
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
// GetCliFilePath()
//==============================================================================

TEST_F(CliPathTest, GetCliFilePathForTheDefaultProtocol) {
  ValidatedContext context(&context_, {kConstraintCliFileProtocol});
  EXPECT_EQ(GetCliFilePath(context, "data\\input.txt"), "data\\input.txt");
  EXPECT_EQ(GetCliFilePath(context, "C:\\src"), "C:\\src");
  EXPECT_EQ(GetCliFilePath(context, ""), "");
}

TEST_F(CliPathTest, GetCliFilePathUsesTheContextsProtocol) {
  context_.SetValue<std::string>(kKeyCliFileProtocol, "mem");
  ValidatedContext context(&context_, {kConstraintCliFileProtocol});
  EXPECT_EQ(GetCliFilePath(context, "data\\input.txt"), "mem:data\\input.txt");
  EXPECT_EQ(GetCliFilePath(context, "/data"), "mem:/data");

  // A path with a protocol of its own keeps it.
  EXPECT_EQ(GetCliFilePath(context, "C:\\src"), "C:\\src");
  EXPECT_EQ(GetCliFilePath(context, "c:src"), "c:src");
  EXPECT_EQ(GetCliFilePath(context, "file:/data"), "file:/data");
  EXPECT_EQ(GetCliFilePath(context, ""), "");
}

TEST_F(CliPathTest, GetCliFilePathWithAnEmptyProtocol) {
  context_.SetValue<std::string>(kKeyCliFileProtocol, "");
  ValidatedContext context(&context_, {kConstraintCliFileProtocol});
  EXPECT_EQ(GetCliFilePath(context, "data\\input.txt"), "data\\input.txt");
}

TEST_F(CliPathTest, GetCliFilePathWithoutTheProtocolConstraint) {
  context_.SetValue<std::string>(kKeyCliFileProtocol, "mem");
  ValidatedContext context(&context_, {kConstraintCliFileSystem});
  EXPECT_EQ(GetCliFilePath(context, "data\\input.txt"), "data\\input.txt");
  EXPECT_THAT(context_errors_, ElementsAre(HasSubstr(kKeyCliFileProtocol)));
}

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

TEST_F(CliPathTest, ValidatorsUseTheContextsProtocol) {
  EXPECT_FALSE(Validate(kCliFileExists, "/mem_only.txt").ok());
  context_.SetValue<std::string>(kKeyCliFileProtocol, "mem");
  EXPECT_TRUE(Validate(kCliFileExists, "/mem_only.txt").ok());
  EXPECT_FALSE(Validate(kCliFileExists, "/data/input.txt").ok());

  // A path with a protocol of its own keeps it.
  EXPECT_TRUE(Validate(kCliFileExists, "file:/data/input.txt").ok());
  EXPECT_TRUE(Validate(kCliFileExists, "C:\\src\\input.txt").ok());
}

TEST_F(CliPathTest, ValidatorsWithAMissingProtocol) {
  context_.SetValue<std::string>(kKeyCliFileProtocol, "missing");
  for (const CliValidator<std::string_view>* validator :
       {&kCliPathExists, &kCliFileExists, &kCliFolderExists}) {
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
