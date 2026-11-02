// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_COMMAND_LINE_H_
#define GB_CLI_CLI_COMMAND_LINE_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_writer.h"

namespace gb {

namespace internal {

// A value given on the command line. A bool flag that is given is true.
using CliValue = std::variant<bool, std::string_view, int64_t, double>;

}  // namespace internal

//==============================================================================
// Context
//
// A command line is parsed and run with a context (see
// gb/base/validated_context.h), which holds what gb_cli and the program's
// commands need from outside it.
//==============================================================================

// The writer for a command line's help and errors. If the context has none,
// WriteCliText() is added to it. A command's run function can write its own
// errors with it too (`(*context.GetPtr<CliWriter>())(CliText::kError, ...)`),
// so that they go where gb_cli's do.
GB_CONTEXT_CONSTRAINT_DEFAULT(kConstraintCliWriter, kInOptional, CliWriter,
                              CliWriter(WriteCliText));

// What gb_cli reads from the context a command line is parsed with.
using CliContract = ContextContract<kConstraintCliWriter>;

//==============================================================================
// CliCommandLine
//==============================================================================

// A parsed command line: the command it names, and the values given for that
// command's flags and arguments and those of the commands above it. This is
// made by ParseCommandLine(), and keeps the context it was parsed with.
//
// String values point into the arguments that were parsed, so the arguments
// must outlive this.
class CliCommandLine {
 public:
  // Returns the program: the root of the command tree.
  const CliCommand* GetProgram() const { return path_.front(); }

  // Returns the command the command line names, which is the one that runs, or
  // the one help is for.
  const CliCommand* GetCommand() const { return path_.back(); }

  // Returns the path from the program to the command the command line names.
  absl::Span<const CliCommand* const> GetCommandPath() const { return path_; }

  // Runs the command with this command line, returning whether it succeeded.
  //
  // The command's run function gets the context, validated against gb_cli's
  // constraints (CliContract) and those of every command in the path. If that
  // fails, as when a required value is missing, the error is reported as
  // ValidatedContext reports any error, and this returns false without running
  // the command. If the command doesn't add a value its constraints say it
  // must, that is reported the same way, and this returns false.
  //
  // If the command line asks for help, with "--help", "-h", or the help command
  // (see cli_help.h), this writes the command's help (FormatCliHelp()) to the
  // context's writer instead, and succeeds. It does the same for a command
  // with no run function.
  bool Run();

  // Returns whether a bool flag was given.
  //
  // The flag must belong to a command in the command path, and the command
  // line must not ask for help (CHECK).
  bool Get(const CliFlag<bool>& flag) const;

  // Returns a flag's value if it was given, else its default value, else
  // std::nullopt.
  //
  // The flag must belong to a command in the command path, and the command
  // line must not ask for help (CHECK).
  template <typename T>
  std::optional<T> Get(const CliFlag<T>& flag) const;

  // Returns an argument's value if it was given, else its default value, else
  // std::nullopt. A required argument is always given.
  //
  // The argument must belong to the command that runs, and the command line
  // must not ask for help (CHECK).
  template <typename T>
  std::optional<T> Get(const CliArg<T>& arg) const;

 private:
  friend absl::StatusOr<CliCommandLine> ParseCommandLine(
      CliContract contract, const CliCommand* program,
      absl::Span<const std::string_view> args);

  CliCommandLine(ValidatedContext context, std::vector<const CliCommand*> path,
                 absl::flat_hash_map<const void*, internal::CliValue> values,
                 bool is_help)
      : context_(std::move(context)),
        path_(std::move(path)),
        values_(std::move(values)),
        is_help_(is_help) {}

  // Returns the value given for `definition`, or null if none was. CHECKs that
  // `definition` belongs to the command path, and that this isn't help.
  const internal::CliValue* Find(const void* definition) const;

  // Returns whether `definition` belongs to a command in the command path.
  bool IsInPath(const void* definition) const;

  ValidatedContext context_;
  std::vector<const CliCommand*> path_;
  absl::flat_hash_map<const void*, internal::CliValue> values_;

  // Whether the command line asks for help. Parsing stops at "--help" or "-h",
  // so values may be missing, including required arguments.
  bool is_help_;
};

// Parses `args`, the arguments after the program's name, as a command line for
// `program`, with the context `contract` holds.
//
// This returns an InvalidArgument error if the command line isn't valid, whose
// message says what is wrong in terms ready to show the user, and ends by
// saying how to get help. `program` must be valid (see ValidateCliCommand(),
// which this CHECKs).
//
// A command line that is valid may ask for help rather than for the command to
// run, so pass it to CliCommandLine::Run(), which handles both, rather than
// reading its values directly.
//
// String values point into `args`, so `args` must outlive the command line. A
// context passed by pointer must outlive it too.
absl::StatusOr<CliCommandLine> ParseCommandLine(
    CliContract contract, const CliCommand* program,
    absl::Span<const std::string_view> args);

// As above, with an empty context.
absl::StatusOr<CliCommandLine> ParseCommandLine(
    const CliCommand* program, absl::Span<const std::string_view> args);

}  // namespace gb

#endif  // GB_CLI_CLI_COMMAND_LINE_H_
