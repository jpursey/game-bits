// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PILOT_SCRIPT_CLI_PILOT_H_
#define GB_PILOT_SCRIPT_CLI_PILOT_H_

#include <memory>
#include <string_view>

#include "absl/status/statusor.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_command_line.h"
#include "gb/cli/cli_path.h"
#include "gb/pilot/pilot.h"

namespace gb {

// The pilot script to run (see ReadPilotScript()): `--pilot <script>`. It must
// be an existing file.
extern const CliFlag<std::string_view> kCliPilotScript;

// The folder the script's saved frames are written to: `--pilot-output
// <folder>`, "out/pilot" if it isn't given.
extern const CliFlag<std::string_view> kCliPilotOutput;

// What CreateCliPilot() reads from the context: the file system the command
// line's paths are opened in.
using CliPilotContract = ContextContract<kConstraintCliFileSystem>;

// Creates a StepPilot that runs the script kCliPilotScript names, and writes
// each frame it saves as `<folder>/<name>.png`, in kCliPilotOutput's folder,
// which is created if it doesn't exist. A command's run function calls this
// with its context, and the command lists both flags.
//
// Returns null if kCliPilotScript isn't given, so the program runs as usual.
// Returns an error, which is logged, if the context doesn't meet the contract,
// the script can't be read, or the output folder isn't a path.
//
// The paths are opened as they are given, in the context's FileSystem, which
// must outlive the pilot. A program using these flags makes the protocol its
// paths are opened in the file system's default, rather than setting
// kConstraintCliFileProtocol.
absl::StatusOr<std::unique_ptr<Pilot>> CreateCliPilot(
    CliPilotContract contract, const CliCommandLine& command_line);

}  // namespace gb

#endif  // GB_PILOT_SCRIPT_CLI_PILOT_H_
