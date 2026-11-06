// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_PATH_H_
#define GB_CLI_CLI_PATH_H_

#include <memory>
#include <string>
#include <string_view>

#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/file/file_system.h"

namespace gb {

//==============================================================================
// Paths on the command line
//
// A path given on a command line is validated as it is written, in the
// context's FileSystem. For paths written the operating system's way, absolute
// or relative to the current directory, main() puts CreateCliFileSystem() in
// the context:
//
//   int main(int argc, char* argv[]) {
//     gb::Context context;
//     context.SetOwned(gb::CreateCliFileSystem());
//     return gb::RunCommandLine(&context, &kProgram,
//                               gb::GetMainArgs(argc, argv));
//   }
//
// The path validators below read the context, and list what they read in
// their own constraints. A command's run function that opens the paths lists
// the file system in its command's constraints:
//
//   const gb::CliArg<std::string_view> kSourceFile = {
//       .name = "source file",
//       .validators = {&gb::kCliFileExists},
//   };
//   const gb::CliCommand kProgram = {
//       .name = "oz3asm",
//       .args = {&kSourceFile},
//       .constraints = {&gb::kConstraintCliFileSystem},
//       .run = Run,
//   };
//==============================================================================

// Returns a file system that opens paths the way the operating system writes
// them: its default protocol, "file", is LocalFileProtocol::CreateOsRoot(), so
// a path without a protocol is absolute or relative to the current directory.
// On Windows, each drive is also a protocol name, so "C:\src\a" opens as it is.
std::unique_ptr<FileSystem> CreateCliFileSystem();

// The file system paths on the command line are opened in.
GB_CONTEXT_CONSTRAINT(kConstraintCliFileSystem, kInRequired, FileSystem);

// The name of the protocol in kConstraintCliFileSystem that the path
// validators check paths without a protocol of their own in ("src/main.txt" is
// checked as "mem:src/main.txt", and "C:\src" as it is). If the context has
// none, or it is empty, they are checked with the file system's default
// protocol. A program sets its own with
// `context.SetValue<std::string>(kKeyCliFileProtocol, name)`, and opens its
// paths under it too, as gb_cli doesn't change the paths it gives a command.
inline constexpr const char* kKeyCliFileProtocol = "cli_file_protocol";
GB_CONTEXT_CONSTRAINT_NAMED(kConstraintCliFileProtocol, kInOptional,
                            std::string, kKeyCliFileProtocol);

// Validate that a path given on the command line exists in
// kConstraintCliFileSystem, is a file, or is a folder. Each reads
// kConstraintCliFileSystem and kConstraintCliFileProtocol, so parsing fails if
// the context has no FileSystem. Parsing also fails, as the program's mistake
// rather than the user's, if the file system doesn't have the protocol that
// kConstraintCliFileProtocol names, or a default protocol if it names none.
extern const CliValidator<std::string_view> kCliPathExists;
extern const CliValidator<std::string_view> kCliFileExists;
extern const CliValidator<std::string_view> kCliFolderExists;

// Validates that a file can be written at a path given on the command line,
// in kConstraintCliFileSystem, without creating any folders: the path names a
// file rather than a folder (it has a file name, doesn't end with a separator,
// and isn't an existing folder), its folder exists (the current folder for a
// bare file name), and its protocol can write the file, and create it if it
// doesn't exist. The file may or may not exist already. This reads the context
// as the validators above do, and fails parsing the same way.
extern const CliValidator<std::string_view> kCliFileWritable;

}  // namespace gb

#endif  // GB_CLI_CLI_PATH_H_
