// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/run_command_line.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gb/base/context.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli_writer.h"

namespace gb {

namespace {

// main()'s exit codes.
constexpr int kExitSuccess = 0;
constexpr int kExitFailure = 1;
constexpr int kExitInvalidCommandLine = 2;

}  // namespace

int RunCommandLine(CliContract contract, const CliCommand* program,
                   absl::Span<const std::string_view> args) {
  // The context is held here, so that it is still there to write an error to
  // if the command line doesn't parse.
  ValidatedContext context(std::move(contract));
  if (!context.IsValid()) {
    // There may be no writer, and ValidatedContext has reported the error.
    return kExitFailure;
  }
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(CliContract(context), program, args);
  if (!command_line.ok()) {
    // The context has a writer, as kConstraintCliWriter has a default.
    (*context.GetPtr<CliWriter>())(CliText::kError,
                                   command_line.status().message());

    // Any other error is the program's, such as its context missing a value.
    return absl::IsInvalidArgument(command_line.status())
               ? kExitInvalidCommandLine
               : kExitFailure;
  }
  return command_line->Run() ? kExitSuccess : kExitFailure;
}

int RunCommandLine(const CliCommand* program,
                   absl::Span<const std::string_view> args) {
  return RunCommandLine(std::make_unique<Context>(), program, args);
}

std::vector<std::string_view> GetMainArgs(int argc, char* argv[]) {
  // argv[0] is the program's name, and argc may be 0.
  if (argc <= 1) {
    return {};
  }
  return std::vector<std::string_view>(argv + 1, argv + argc);
}

}  // namespace gb
