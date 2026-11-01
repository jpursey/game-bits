// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_values.h"

#include <string_view>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "gb/cli/cli_internal.h"

namespace gb {

namespace {

//==============================================================================
// CliParser
//==============================================================================

// Parses a command line in a single pass, walking down the command tree as it
// reaches each subcommand's name.
class CliParser {
 public:
  explicit CliParser(const CliCommand* program) : path_({program}) {}

  absl::Status Parse(absl::Span<const std::string> args);

  std::vector<const CliCommand*> TakePath() { return std::move(path_); }
  absl::flat_hash_map<const void*, internal::CliValue> TakeValues() {
    return std::move(values_);
  }

 private:
  // Parses the flag at `args[*index]`, advancing `*index` past its value if it
  // takes the next argument.
  absl::Status ParseFlag(absl::Span<const std::string> args, int* index);

  // Parses a bare word: a subcommand's name, or a positional argument.
  absl::Status ParseWord(std::string_view word);

  // Validates that nothing required is missing, once all arguments are
  // parsed.
  absl::Status ValidateComplete() const;

  // Return the flag in the command path with this long or short name, or null
  // if there is none.
  const internal::CliFlagRef* FindFlag(std::string_view name) const;
  const internal::CliFlagRef* FindFlag(char short_name) const;

  // Returns an error for the command at the end of the path.
  absl::Status Error(std::string_view message) const;

  std::vector<const CliCommand*> path_;
  absl::flat_hash_map<const void*, internal::CliValue> values_;

  // The index of the next positional argument of the command at the end of
  // the path.
  int next_arg_ = 0;

  // Whether "--" was given, so the remaining arguments are all positional.
  bool positional_only_ = false;
};

absl::Status CliParser::Parse(absl::Span<const std::string> args) {
  const int arg_count = static_cast<int>(args.size());
  for (int i = 0; i < arg_count; ++i) {
    const std::string_view arg = args[i];
    absl::Status status;
    if (positional_only_ || arg == "-" || !arg.starts_with('-')) {
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
  return ValidateComplete();
}

absl::Status CliParser::ParseFlag(absl::Span<const std::string> args,
                                  int* index) {
  const std::string_view arg = args[*index];

  // How the flag was written, without any "=value", for errors.
  std::string_view flag_text = arg;
  std::optional<std::string_view> attached_value;
  const internal::CliFlagRef* flag = nullptr;
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
  const void* const definition = flag->GetDefinition();
  if (values_.contains(definition)) {
    return Error(absl::StrCat("\"", flag_text, "\" was given twice"));
  }

  if (!flag->TakesValue()) {
    if (attached_value.has_value()) {
      return Error(absl::StrCat("\"", flag_text, "\" doesn't take a value"));
    }
    values_[definition] = true;
    return absl::OkStatus();
  }

  std::string_view text;
  if (attached_value.has_value()) {
    text = *attached_value;
  } else if (*index + 1 < static_cast<int>(args.size())) {
    text = args[++*index];
  } else {
    return Error(absl::StrCat("\"", flag_text, "\" needs a value"));
  }
  absl::StatusOr<internal::CliValue> value = flag->Parse(text);
  if (!value.ok()) {
    return Error(
        absl::StrCat("\"", flag_text, "\": ", value.status().message()));
  }
  values_[definition] = *std::move(value);
  return absl::OkStatus();
}

absl::Status CliParser::ParseWord(std::string_view word) {
  const CliCommand* command = path_.back();
  if (command->commands.size() != 0 && !positional_only_) {
    for (const CliCommand* subcommand : command->commands) {
      if (subcommand->name == word) {
        path_.push_back(subcommand);
        return absl::OkStatus();
      }
    }
    return Error(absl::StrCat("unknown command \"", word, "\""));
  }

  if (next_arg_ == static_cast<int>(command->args.size())) {
    return Error(absl::StrCat("unexpected argument \"", word, "\""));
  }
  const internal::CliArgRef& arg = command->args.begin()[next_arg_];
  absl::StatusOr<internal::CliValue> value = arg.Parse(word);
  if (!value.ok()) {
    return Error(absl::StrCat("<", arg.GetFields().name,
                              ">: ", value.status().message()));
  }
  values_[arg.GetDefinition()] = *std::move(value);
  ++next_arg_;
  return absl::OkStatus();
}

absl::Status CliParser::ValidateComplete() const {
  const CliCommand* command = path_.back();
  if (command->commands.size() != 0 && command->run == nullptr) {
    return Error("missing command");
  }
  if (next_arg_ < static_cast<int>(command->args.size())) {
    const internal::CliArgFields fields =
        command->args.begin()[next_arg_].GetFields();
    if (!fields.optional) {
      return Error(absl::StrCat("missing <", fields.name, ">"));
    }
  }
  return absl::OkStatus();
}

const internal::CliFlagRef* CliParser::FindFlag(std::string_view name) const {
  for (const CliCommand* command : path_) {
    for (const internal::CliFlagRef& flag : command->flags) {
      if (flag.GetFields().name == name) {
        return &flag;
      }
    }
  }
  return nullptr;
}

const internal::CliFlagRef* CliParser::FindFlag(char short_name) const {
  for (const CliCommand* command : path_) {
    for (const internal::CliFlagRef& flag : command->flags) {
      if (flag.GetFields().short_name == short_name) {
        return &flag;
      }
    }
  }
  return nullptr;
}

absl::Status CliParser::Error(std::string_view message) const {
  return absl::InvalidArgumentError(
      absl::StrCat(internal::GetCliPathName(path_), ": ", message));
}

}  // namespace

//==============================================================================
// CliValues
//==============================================================================

bool CliValues::Get(const CliFlag<bool>& flag) const {
  return Find(&flag) != nullptr;
}

const internal::CliValue* CliValues::Find(const void* definition) const {
  const auto it = values_.find(definition);
  if (it != values_.end()) {
    return &it->second;
  }
  CHECK(IsInPath(definition))
      << "Flag or argument read from a command line for \""
      << internal::GetCliPathName(path_) << "\", which it doesn't belong to";
  return nullptr;
}

bool CliValues::IsInPath(const void* definition) const {
  for (const CliCommand* command : path_) {
    for (const internal::CliFlagRef& flag : command->flags) {
      if (flag.GetDefinition() == definition) {
        return true;
      }
    }
    for (const internal::CliArgRef& arg : command->args) {
      if (arg.GetDefinition() == definition) {
        return true;
      }
    }
  }
  return false;
}

//==============================================================================
// ParseCommandLine
//==============================================================================

absl::StatusOr<CliValues> ParseCommandLine(const CliCommand* program,
                                           absl::Span<const std::string> args) {
  CHECK_OK(ValidateCliCommand(program));
  CliParser parser(program);
  const absl::Status status = parser.Parse(args);
  if (!status.ok()) {
    return status;
  }
  return CliValues(parser.TakePath(), parser.TakeValues());
}

}  // namespace gb
