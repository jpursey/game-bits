// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_PATH_H_
#define GB_CLI_CLI_PATH_H_

#include <string>
#include <string_view>

#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/file/file_system.h"

namespace gb {

//==============================================================================
// Paths on the command line
//
// A path given on a command line is written the operating system's way,
// absolute or relative to the current directory. A program opens it in the
// context's FileSystem, with a protocol that takes the paths of a protocol from
// LocalFileProtocol::CreateOsRoot(), as GetCliFilePath() converts it: the file
// system's default protocol, unless the context names another.
//
// The path validators below read the context, as does a command's run function
// that calls GetCliFilePath(), so each lists both constraints:
//
//   const gb::CliArg<std::string_view> kSourceFile = {
//       .name = "source file",
//       .validators = {&gb::kCliFileExists},
//   };
//   const gb::CliCommand kProgram = {
//       .name = "oz3asm",
//       .args = {&kSourceFile},
//       .constraints = {&gb::kConstraintCliFileSystem,
//                       &gb::kConstraintCliFileProtocol},
//       .run = Run,
//   };
//==============================================================================

// The file system paths on the command line are opened in.
GB_CONTEXT_CONSTRAINT(kConstraintCliFileSystem, kInRequired, FileSystem);

// The name of the protocol in kConstraintCliFileSystem that paths on the
// command line are opened with. If the context has none, or it is empty, they
// are opened with the file system's default protocol. A program sets its own
// with `context.SetValue<std::string>(kKeyCliFileProtocol, name)`.
inline constexpr const char* kKeyCliFileProtocol = "cli_file_protocol";
GB_CONTEXT_CONSTRAINT_NAMED(kConstraintCliFileProtocol, kInOptional,
                            std::string, kKeyCliFileProtocol);

// Returns the path in kConstraintCliFileSystem of `path`, a path given on the
// command line: converted by LocalFileProtocol::ToOsRootPath(), and prefixed
// with the protocol in kConstraintCliFileProtocol if there is one
// ("file:/c/src/main.txt", or "/c/src/main.txt" for the default protocol).
//
// Returns an empty string if the path can't be converted (an empty path, or a
// network path). If `context` doesn't allow kConstraintCliFileProtocol to be
// read, that is reported as a context error, and the path is for the default
// protocol.
std::string GetCliFilePath(const ValidatedContext& context,
                           std::string_view path);

// Validate that a path given on the command line exists in
// kConstraintCliFileSystem, is a file, or is a folder. Each reads
// kConstraintCliFileSystem and kConstraintCliFileProtocol, so parsing fails if
// the context has no FileSystem. Parsing also fails, as the program's mistake
// rather than the user's, if the file system doesn't have the protocol that
// kConstraintCliFileProtocol names, or a default protocol if it names none.
extern const CliValidator<std::string_view> kCliPathExists;
extern const CliValidator<std::string_view> kCliFileExists;
extern const CliValidator<std::string_view> kCliFolderExists;

}  // namespace gb

#endif  // GB_CLI_CLI_PATH_H_
