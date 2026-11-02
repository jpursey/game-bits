// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_HELP_H_
#define GB_CLI_CLI_HELP_H_

#include <string>

#include "absl/types/span.h"
#include "gb/cli/cli.h"

namespace gb {

//==============================================================================
// Command line help
//==============================================================================

// A "help" command, which a program may add to any command's subcommands.
// `prog help build` asks for the same help as `prog build --help`, and
// `prog help` the same as `prog --help`: CliCommandLine::Run() writes the help
// for the command named after "help".
extern const CliCommand kCliHelpCommand;

// Returns the help for the command at the end of `path`, the commands from the
// program down to it (CliCommandLine::GetCommandPath()), wrapped to `width`
// columns: how to use it, its help, then its arguments or subcommands, and its
// flags, followed by those of the commands above it.
//
// A definition's help is UTF-8, and is split into paragraphs at blank lines
// ("\n\n"). A subcommand is listed with the first paragraph of its help.
//
// Any width works, but a narrow one puts a word on each line, and a word wider
// than a line runs past it.
std::string FormatCliHelp(absl::Span<const CliCommand* const> path, int width);

}  // namespace gb

#endif  // GB_CLI_CLI_HELP_H_
