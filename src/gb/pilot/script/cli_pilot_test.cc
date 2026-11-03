// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/script/cli_pilot.h"

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
#include "gb/file/file_types.h"
#include "gb/file/memory_file_protocol.h"
#include "gb/image/image.h"
#include "gb/image/image_file.h"
#include "gb/image/pixel.h"
#include "gb/pilot/pilot.h"
#include "gb/test/log_error_guard.h"
#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::StartsWith;

const CliCommand kProgram = {
    .name = "prog",
    .flags = {&kCliPilotScript, &kCliPilotOutput},
};

auto LogTextHas(std::string_view text) {
  return Field(&LogRecorder::Message::text, HasSubstr(std::string(text)));
}

// A memory protocol whose current folder is "/data".
class CurrentFolderProtocol : public MemoryFileProtocol {
 public:
  CurrentFolderProtocol()
      : MemoryFileProtocol(kReadWriteFileProtocolFlags +
                           FileProtocolFlag::kCurrentPath) {}

 protected:
  std::string DoGetCurrentPath(std::string_view) override { return "/data"; }
};

// Holds a file system with a memory protocol as its default, whose current
// folder is "/data", and a context holding it.
class CliPilotTest : public ::testing::Test {
 protected:
  CliPilotTest() {
    EXPECT_TRUE(file_system_.Register(std::make_unique<CurrentFolderProtocol>(),
                                      "mem"));
    EXPECT_TRUE(file_system_.SetDefaultProtocol("mem"));
    EXPECT_TRUE(file_system_.CreateFolder("/data"));
    context_.SetPtr<FileSystem>(&file_system_);
  }

  // Parses `args` as kProgram's command line, and creates its pilot with the
  // context as its run function gets it, validated against no constraints of
  // its own.
  absl::StatusOr<std::unique_ptr<Pilot>> CreatePilot(
      const std::vector<std::string_view>& args) {
    absl::StatusOr<CliCommandLine> command_line =
        ParseCommandLine(&context_, &kProgram, args);
    if (!command_line.ok()) {
      ADD_FAILURE() << command_line.status();
      return command_line.status();
    }
    ValidatedContext context(&context_, {});
    return CreateCliPilot(context, *command_line);
  }

  // Runs frames until the pilot is done, drawing drawn_ in each, and returns
  // the frames run.
  int RunUntilDone(Pilot* pilot) {
    int frames = 0;
    while (!pilot->IsDone() && frames < 100) {
      pilot->BeginFrame();
      if (pilot->IsSavingFrame()) {
        pilot->SaveFrame(drawn_);
      }
      pilot->EndFrame();
      ++frames;
    }
    return frames;
  }

  const Image drawn_{2, 1, Pixel(1, 2, 3, 4)};
  LogErrorGuard log_error_guard_;
  FileSystem file_system_;
  Context context_;
};

TEST_F(CliPilotTest, NoScriptGivesNoPilot) {
  absl::StatusOr<std::unique_ptr<Pilot>> pilot = CreatePilot({});
  ASSERT_TRUE(pilot.ok()) << pilot.status();
  EXPECT_EQ(*pilot, nullptr);
}

TEST_F(CliPilotTest, ContextWithoutAFileSystemFails) {
  const std::vector<std::string_view> args;
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&context_, &kProgram, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  Context context;
  absl::StatusOr<std::unique_ptr<Pilot>> pilot =
      CreateCliPilot(&context, *command_line);
  EXPECT_TRUE(absl::IsFailedPrecondition(pilot.status())) << pilot.status();
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogTextHas("FileSystem")));
}

TEST_F(CliPilotTest, ScriptMustExist) {
  const std::vector<std::string_view> args = {"--pilot", "/data/run.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&context_, &kProgram, args);
  EXPECT_THAT(
      command_line.status().message(),
      StartsWith("prog: \"--pilot\": \"/data/run.txt\" doesn't exist."));
}

TEST_F(CliPilotTest, ScriptWithErrorsFails) {
  ASSERT_TRUE(file_system_.WriteFile("/data/run.txt", "wait 1\nwiat 3\n"));
  absl::StatusOr<std::unique_ptr<Pilot>> pilot =
      CreatePilot({"--pilot", "/data/run.txt"});
  EXPECT_TRUE(absl::IsInvalidArgument(pilot.status())) << pilot.status();
  EXPECT_EQ(pilot.status().message(),
            "/data/run.txt(2:1): Parse error at \"wiat\": Unknown step");
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogTextHas("Unknown step")));
}

TEST_F(CliPilotTest, RunsTheScriptAndSavesFrames) {
  ASSERT_TRUE(file_system_.WriteFile(
      "/data/run.txt", "hide window\nwait 2\nsave start\nsave \"end 1\"\n"));
  absl::StatusOr<std::unique_ptr<Pilot>> pilot = CreatePilot(
      {"--pilot", "/data/run.txt", "--pilot-output", "/frames/run"});
  ASSERT_TRUE(pilot.ok()) << pilot.status();
  ASSERT_NE(*pilot, nullptr);
  EXPECT_TRUE((*pilot)->IsWindowHidden());

  EXPECT_EQ(RunUntilDone(pilot->get()), 4);
  EXPECT_FALSE((*pilot)->HasFailed());
  for (std::string_view path :
       {"/frames/run/start.png", "/frames/run/end 1.png"}) {
    std::unique_ptr<Image> saved = LoadImage(&file_system_, path);
    ASSERT_NE(saved, nullptr) << path;
    EXPECT_THAT(saved->GetPixels(),
                ElementsAre(Pixel(1, 2, 3, 4), Pixel(1, 2, 3, 4)))
        << path;
  }
}

TEST_F(CliPilotTest, SavesFramesInOutPilotByDefault) {
  ASSERT_TRUE(file_system_.WriteFile("run.txt", "save start"));
  absl::StatusOr<std::unique_ptr<Pilot>> pilot =
      CreatePilot({"--pilot", "run.txt"});
  ASSERT_TRUE(pilot.ok()) << pilot.status();
  ASSERT_NE(*pilot, nullptr);
  EXPECT_FALSE((*pilot)->IsWindowHidden());

  RunUntilDone(pilot->get());
  EXPECT_FALSE((*pilot)->HasFailed());
  EXPECT_TRUE(file_system_.IsValidFile("/data/out/pilot/start.png"));
}

TEST_F(CliPilotTest, SavesFramesInTheRootFolder) {
  ASSERT_TRUE(file_system_.WriteFile("/data/run.txt", "save start"));
  absl::StatusOr<std::unique_ptr<Pilot>> pilot =
      CreatePilot({"--pilot", "/data/run.txt", "--pilot-output", "/"});
  ASSERT_TRUE(pilot.ok()) << pilot.status();
  ASSERT_NE(*pilot, nullptr);

  RunUntilDone(pilot->get());
  EXPECT_FALSE((*pilot)->HasFailed());
  EXPECT_TRUE(file_system_.IsValidFile("/start.png"));
}

TEST_F(CliPilotTest, OutputThatIsntAPathFails) {
  ASSERT_TRUE(file_system_.WriteFile("/data/run.txt", "save start"));
  absl::StatusOr<std::unique_ptr<Pilot>> pilot =
      CreatePilot({"--pilot", "/data/run.txt", "--pilot-output", "//"});
  EXPECT_TRUE(absl::IsInvalidArgument(pilot.status())) << pilot.status();
  EXPECT_EQ(pilot.status().message(),
            "The pilot output folder \"//\" isn't a path");
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogTextHas("isn't a path")));
}

TEST_F(CliPilotTest, FrameThatCantBeWrittenFails) {
  ASSERT_TRUE(file_system_.WriteFile("/data/run.txt", "save start"));
  ASSERT_TRUE(file_system_.WriteFile("/data/blocked", ""));
  absl::StatusOr<std::unique_ptr<Pilot>> pilot = CreatePilot(
      {"--pilot", "/data/run.txt", "--pilot-output", "/data/blocked"});
  ASSERT_TRUE(pilot.ok()) << pilot.status();
  ASSERT_NE(*pilot, nullptr);

  RunUntilDone(pilot->get());
  EXPECT_TRUE((*pilot)->HasFailed());
  EXPECT_THAT(
      log_error_guard_.TakeMessages(),
      ElementsAre(
          LogTextHas("The pilot output folder \"/data/blocked\" can't be "
                     "created"),
          LogTextHas("save start failed")));
}

}  // namespace
}  // namespace gb
