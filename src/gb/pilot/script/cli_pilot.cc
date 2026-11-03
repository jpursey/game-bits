// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/script/cli_pilot.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gb/cli/cli_path.h"
#include "gb/cli/cli_validators.h"
#include "gb/file/file_system.h"
#include "gb/file/file_types.h"
#include "gb/file/path.h"
#include "gb/image/image.h"
#include "gb/image/image_file.h"
#include "gb/pilot/script/pilot_script.h"
#include "gb/pilot/step_pilot.h"

namespace gb {

namespace {

// Logs `status`'s message as an error, and returns it.
absl::Status LogError(absl::Status status) {
  LOG(ERROR) << status.message();
  return status;
}

// Writes `image` as `<folder>/<name>.png`, creating the folder if it doesn't
// exist. `folder` is normalized. Returns false, having logged why, if it can't.
bool SavePilotFrame(FileSystem* file_system, std::string_view folder,
                    std::string_view name, const Image& image) {
  if (!file_system->CreateFolder(folder, FolderMode::kRecursive)) {
    LOG(ERROR) << "The pilot output folder \"" << folder
               << "\" can't be created";
    return false;
  }
  return SaveImage(file_system, JoinPath(folder, absl::StrCat(name, ".png")),
                   image.View());
}

}  // namespace

const CliFlag<std::string_view> kCliPilotScript = {
    .name = "pilot",
    .value_name = "script",
    .help =
        "Runs the program with a pilot script, which gives it its input "
        "and saves frames, then exits. The exit code says whether every "
        "step passed.",
    .validators = {&kCliFileExists},
};

const CliFlag<std::string_view> kCliPilotOutput = {
    .name = "pilot-output",
    .value_name = "folder",
    .help =
        "Where the pilot script's saved frames are written, each as "
        "<name>.png.",
    .default_value = "out/pilot",
    .validators = {&kCliNotEmpty},
};

absl::StatusOr<std::unique_ptr<Pilot>> CreateCliPilot(
    CliPilotContract contract, const CliCommandLine& command_line) {
  // ValidatedContext has already reported what the context is missing.
  ValidatedContext context = std::move(contract);
  if (!context.IsValid()) {
    return absl::FailedPreconditionError(
        "The context doesn't meet CreateCliPilot()'s contract");
  }
  const std::optional<std::string_view> script_path =
      command_line.Get(kCliPilotScript);
  if (!script_path.has_value()) {
    return nullptr;
  }

  FileSystem* file_system = context.GetPtr<FileSystem>();
  std::string text;
  if (!file_system->ReadFile(*script_path, &text)) {
    return LogError(absl::InvalidArgumentError(
        absl::StrCat("The pilot script \"", *script_path, "\" can't be read")));
  }
  absl::StatusOr<PilotScript> script =
      ReadPilotScript(std::move(text), *script_path);
  if (!script.ok()) {
    return LogError(script.status());
  }

  // A path that can't be normalized gives an empty folder, which would be the
  // current one.
  const std::string_view output = *command_line.Get(kCliPilotOutput);
  PathFlags failed_flag;
  std::string folder = NormalizePath(output, &failed_flag);
  if (!failed_flag.IsEmpty()) {
    return LogError(absl::InvalidArgumentError(
        absl::StrCat("The pilot output folder \"", output, "\" isn't a path")));
  }

  auto pilot = std::make_unique<StepPilot>(StepPilot::Options{
      .hide_window = script->hide_window,
      .save_frame =
          [file_system, folder = std::move(folder)](std::string_view name,
                                                    const Image& image) {
            return SavePilotFrame(file_system, folder, name, image);
          },
  });
  pilot->AddSteps(std::move(script->steps));
  return pilot;
}

}  // namespace gb
