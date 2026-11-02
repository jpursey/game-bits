// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_RUN_COMMAND_LINE_H_
#define GB_CLI_RUN_COMMAND_LINE_H_

#include <string_view>
#include <vector>

#include "absl/types/span.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_command_line.h"

namespace gb {

//==============================================================================
// Running a command line
//
// A program's main() parses and runs its command line in one call:
//
//   int main(int argc, char* argv[]) {
//     return gb::RunCommandLine(&kProgram, gb::GetMainArgs(argc, argv));
//   }
//==============================================================================

// Parses `args`, the arguments after the program's name, as a command line for
// `program` with the context `contract` holds, and runs it
// (CliCommandLine::Run()). If the command line isn't valid, this writes what is
// wrong to the context's writer instead (see kConstraintCliWriter).
//
// Returns main()'s exit code: 0 if the command succeeded or help was written,
// 1 if the command failed, and 2 if the command line isn't valid.
int RunCommandLine(CliContract contract, const CliCommand* program,
                   absl::Span<const std::string_view> args);

// As above, with an empty context.
int RunCommandLine(const CliCommand* program,
                   absl::Span<const std::string_view> args);

// Returns main()'s arguments after the program's name, which stay valid for the
// rest of the program.
//
// The arguments are as main() was given them, so on Windows they are in the
// ANSI code page rather than UTF-8.
std::vector<std::string_view> GetMainArgs(int argc, char* argv[]);

}  // namespace gb

#endif  // GB_CLI_RUN_COMMAND_LINE_H_
