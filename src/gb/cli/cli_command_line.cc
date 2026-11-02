// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_command_line.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "gb/base/context.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli_help.h"
#include "gb/cli/cli_internal.h"

namespace gb {

namespace {

//==============================================================================
// CliParser
//==============================================================================

// Returns whether `arg` is a bare word, rather than a flag: anything not
// starting with "-", "-" alone, and a negative number ("-5", "-.5"), as no
// short name is a digit or ".".
bool IsWord(std::string_view arg) {
  if (!arg.starts_with('-') || arg == "-") {
    return true;
  }
  return absl::ascii_isdigit(arg[1]) || arg[1] == '.';
}

// The help flag, as every command lists it.
const CliAnyFlag kHelpFlag = &internal::kCliHelpFlag;

// Parses a command line in a single pass, walking down the command tree as it
// reaches each subcommand's name.
class CliParser {
 public:
  // Parses with `context`, which must outlive this.
  CliParser(ValidatedContext* context, const CliCommand* program)
      : context_(context), path_({program}) {}

  absl::Status Parse(absl::Span<const std::string_view> args);

  bool IsHelp() const { return help_flag_given_ || help_command_given_; }
  std::vector<const CliCommand*> TakePath() { return std::move(path_); }
  absl::flat_hash_map<const void*, internal::CliValue> TakeValues() {
    return std::move(values_);
  }

 private:
  // Parses the flag at `args[*index]`, advancing `*index` past its value if it
  // takes the next argument.
  absl::Status ParseFlag(absl::Span<const std::string_view> args, int* index);

  // Parses a bare word: a subcommand's name, or a positional argument. After
  // the help command, every word is a subcommand's name.
  absl::Status ParseWord(std::string_view word);

  // Validates the context against the constraints of the validators of the
  // flags and arguments of every command in the path, so a missing value is
  // found when the command is reached, whether or not they are given. Each
  // validator gets its own context, with only its own constraints (see
  // internal::ParseCliValue()).
  absl::Status ValidateContext();

  // Validates that nothing required is missing, once all arguments are
  // parsed.
  absl::Status ValidateComplete() const;

  // Return the flag in the command path with this long or short name,
  // including the help flag, or null if there is none.
  const CliAnyFlag* FindFlag(std::string_view name) const;
  const CliAnyFlag* FindFlag(char short_name) const;

  // Returns an error for the command at the end of the path.
  absl::Status Error(std::string_view message) const;

  ValidatedContext* const context_;
  std::vector<const CliCommand*> path_;
  absl::flat_hash_map<const void*, internal::CliValue> values_;

  // The index of the next positional argument of the command at the end of
  // the path.
  int next_arg_ = 0;

  // Whether "--" was given, so the remaining arguments are all positional.
  bool positional_only_ = false;

  // Whether the help flag was given. Help is for the command so far, so the
  // rest of the command line isn't parsed.
  bool help_flag_given_ = false;

  // Whether the help command was given, so every word after it names a
  // subcommand.
  bool help_command_given_ = false;
};

absl::Status CliParser::Parse(absl::Span<const std::string_view> args) {
  absl::Status status = ValidateContext();
  if (!status.ok()) {
    return status;
  }
  const int arg_count = static_cast<int>(args.size());
  for (int i = 0; i < arg_count && !help_flag_given_; ++i) {
    const std::string_view arg = args[i];
    if (positional_only_ || IsWord(arg)) {
      status = ParseWord(arg);
    } else if (arg == "--") {
      positional_only_ = true;
    } else {
      status = ParseFlag(args, &i);
    }
    if (!status.ok()) {
      return status;
    }
  }
  if (IsHelp()) {
    return absl::OkStatus();
  }
  return ValidateComplete();
}

absl::Status CliParser::ParseFlag(absl::Span<const std::string_view> args,
                                  int* index) {
  const std::string_view arg = args[*index];

  // How the flag was written, without any "=value", for errors.
  std::string_view flag_text = arg;
  std::optional<std::string_view> attached_value;
  const CliAnyFlag* flag = nullptr;
  if (arg.starts_with("--")) {
    const std::vector<std::string_view> parts =
        absl::StrSplit(arg, absl::MaxSplits('=', 1));
    flag_text = parts[0];
    if (parts.size() == 2) {
      attached_value = parts[1];
    }
    flag = FindFlag(flag_text.substr(2));
  } else if (arg.size() == 2) {
    flag = FindFlag(arg[1]);
  }
  if (flag == nullptr) {
    return Error(absl::StrCat("unknown flag \"", flag_text, "\""));
  }
  const internal::CliFlagFields fields = internal::GetCliFlagFields(*flag);
  if (values_.contains(fields.definition)) {
    return Error(absl::StrCat("\"", flag_text, "\" was given twice"));
  }

  std::string_view text;
  if (std::holds_alternative<const CliFlag<bool>*>(*flag)) {
    if (attached_value.has_value()) {
      return Error(absl::StrCat("\"", flag_text, "\" doesn't take a value"));
    }
    if (flag == &kHelpFlag) {
      help_flag_given_ = true;
      return absl::OkStatus();
    }
  } else if (attached_value.has_value()) {
    text = *attached_value;
  } else if (*index + 1 < static_cast<int>(args.size())) {
    text = args[++*index];
  } else {
    return Error(absl::StrCat("\"", flag_text, "\" needs a value"));
  }
  absl::StatusOr<internal::CliValue> value =
      internal::ParseCliValue(context_, *flag, text);
  if (!value.ok()) {
    return Error(
        absl::StrCat("\"", flag_text, "\": ", value.status().message()));
  }
  values_[fields.definition] = *std::move(value);
  return absl::OkStatus();
}

absl::Status CliParser::ParseWord(std::string_view word) {
  const CliCommand* command = path_.back();
  if (help_command_given_ ||
      (command->commands.size() != 0 && !positional_only_)) {
    for (const CliCommand* subcommand : command->commands) {
      if (subcommand->name != word) {
        continue;
      }

      // The help command asks for help for the commands named after it, so
      // it isn't in the path itself, unless it is named again after itself
      // (`prog help help`).
      if (subcommand == &kCliHelpCommand && !help_command_given_) {
        help_command_given_ = true;
        return absl::OkStatus();
      }
      path_.push_back(subcommand);
      return ValidateContext();
    }
    return Error(absl::StrCat("unknown command \"", word, "\""));
  }

  if (next_arg_ == static_cast<int>(command->args.size())) {
    return Error(absl::StrCat("unexpected argument \"", word, "\""));
  }
  const CliAnyArg& arg = command->args.begin()[next_arg_];
  const internal::CliArgFields fields = internal::GetCliArgFields(arg);
  absl::StatusOr<internal::CliValue> value =
      internal::ParseCliValue(context_, arg, word);
  if (!value.ok()) {
    return Error(
        absl::StrCat("<", fields.name, ">: ", value.status().message()));
  }
  values_[fields.definition] = *std::move(value);
  ++next_arg_;
  return absl::OkStatus();
}

absl::Status CliParser::ValidateContext() {
  std::vector<ContextConstraint> constraints;
  for (const CliCommand* command : path_) {
    for (const CliAnyFlag& flag : command->flags) {
      internal::AppendCliValidatorConstraints(flag, &constraints);
    }
    for (const CliAnyArg& arg : command->args) {
      internal::AppendCliValidatorConstraints(arg, &constraints);
    }
  }
  const ValidatedContext validator_context(*context_, std::move(constraints));
  if (!validator_context.IsValid()) {
    return absl::FailedPreconditionError(
        absl::StrCat(internal::GetCliPathName(path_),
                     ": the context is missing a value that the validators "
                     "of its flags or arguments read"));
  }
  return absl::OkStatus();
}

absl::Status CliParser::ValidateComplete() const {
  const CliCommand* command = path_.back();
  if (next_arg_ < static_cast<int>(command->args.size())) {
    const internal::CliArgFields fields =
        internal::GetCliArgFields(command->args.begin()[next_arg_]);
    if (!fields.optional) {
      return Error(absl::StrCat("missing <", fields.name, ">"));
    }
  }
  return absl::OkStatus();
}

const CliAnyFlag* CliParser::FindFlag(std::string_view name) const {
  if (name == internal::kCliHelpFlag.name) {
    return &kHelpFlag;
  }
  for (const CliCommand* command : path_) {
    for (const CliAnyFlag& flag : command->flags) {
      if (internal::GetCliFlagFields(flag).name == name) {
        return &flag;
      }
    }
  }
  return nullptr;
}

const CliAnyFlag* CliParser::FindFlag(char short_name) const {
  if (short_name == internal::kCliHelpFlag.short_name) {
    return &kHelpFlag;
  }
  for (const CliCommand* command : path_) {
    for (const CliAnyFlag& flag : command->flags) {
      if (internal::GetCliFlagFields(flag).short_name == short_name) {
        return &flag;
      }
    }
  }
  return nullptr;
}

absl::Status CliParser::Error(std::string_view message) const {
  const std::string path_name = internal::GetCliPathName(path_);
  return absl::InvalidArgumentError(absl::StrCat(
      path_name, ": ", message, ". Run \"", path_name, " --help\" for help."));
}

}  // namespace

//==============================================================================
// CliCommandLine
//==============================================================================

bool CliCommandLine::Run() {
  const CliCommand* command = GetCommand();
  if (is_help_ || command->run == nullptr) {
    // The context has a writer and a width, as their constraints have
    // defaults.
    (*context_.GetPtr<CliWriter>())(
        CliText::kHelp,
        FormatCliHelp(path_, context_.GetValue<int>(kKeyCliHelpWidth)));
    return true;
  }
  std::vector<ContextConstraint> constraints = CliContract::GetConstraints();
  for (const CliCommand* path_command : path_) {
    for (const ContextConstraint* constraint : path_command->constraints) {
      constraints.push_back(*constraint);
    }
  }
  ValidatedContext command_context(context_, std::move(constraints));
  if (!command_context.IsValid()) {
    return false;
  }

  // The command fails if it doesn't add a value it must. Completing the
  // context, when it is destroyed, reports which.
  const bool succeeded = command->run(command_context, *this);
  return command_context.IsValidToComplete() && succeeded;
}

bool CliCommandLine::Get(const CliFlag<bool>& flag) const {
  return Find(&flag) != nullptr;
}

template <typename T>
std::optional<T> CliCommandLine::Get(const CliFlag<T>& flag) const {
  const internal::CliValue* value = Find(&flag);
  if (value == nullptr) {
    return flag.default_value;
  }
  return *std::get_if<T>(value);
}

template <typename T>
std::optional<T> CliCommandLine::Get(const CliArg<T>& arg) const {
  const internal::CliValue* value = Find(&arg);
  if (value == nullptr) {
    return arg.default_value;
  }
  return *std::get_if<T>(value);
}

template std::optional<std::string_view> CliCommandLine::Get(
    const CliFlag<std::string_view>& flag) const;
template std::optional<int64_t> CliCommandLine::Get(
    const CliFlag<int64_t>& flag) const;
template std::optional<double> CliCommandLine::Get(
    const CliFlag<double>& flag) const;
template std::optional<std::string_view> CliCommandLine::Get(
    const CliArg<std::string_view>& arg) const;
template std::optional<int64_t> CliCommandLine::Get(
    const CliArg<int64_t>& arg) const;
template std::optional<double> CliCommandLine::Get(
    const CliArg<double>& arg) const;

const internal::CliValue* CliCommandLine::Find(const void* definition) const {
  CHECK(!is_help_) << "Flag or argument read from a command line for \""
                   << internal::GetCliPathName(path_)
                   << "\" that asks for help";
  const auto it = values_.find(definition);
  if (it != values_.end()) {
    return &it->second;
  }
  CHECK(IsInPath(definition))
      << "Flag or argument read from a command line for \""
      << internal::GetCliPathName(path_) << "\", which it doesn't belong to";
  return nullptr;
}

bool CliCommandLine::IsInPath(const void* definition) const {
  for (const CliCommand* command : path_) {
    for (const CliAnyFlag& flag : command->flags) {
      if (internal::GetCliFlagFields(flag).definition == definition) {
        return true;
      }
    }
    for (const CliAnyArg& arg : command->args) {
      if (internal::GetCliArgFields(arg).definition == definition) {
        return true;
      }
    }
  }
  return false;
}

//==============================================================================
// ParseCommandLine
//==============================================================================

absl::StatusOr<CliCommandLine> ParseCommandLine(
    CliContract contract, const CliCommand* program,
    absl::Span<const std::string_view> args) {
  CHECK_OK(ValidateCliCommand(program));
  ValidatedContext context(std::move(contract));
  if (!context.IsValid()) {
    return absl::FailedPreconditionError(absl::StrCat(
        program->name, ": the context doesn't meet gb_cli's constraints"));
  }
  CliParser parser(&context, program);
  const absl::Status status = parser.Parse(args);
  if (!status.ok()) {
    return status;
  }
  return CliCommandLine(std::move(context), parser.TakePath(),
                        parser.TakeValues(), parser.IsHelp());
}

absl::StatusOr<CliCommandLine> ParseCommandLine(
    const CliCommand* program, absl::Span<const std::string_view> args) {
  return ParseCommandLine(std::make_unique<Context>(), program, args);
}

}  // namespace gb
