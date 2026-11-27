// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_INTERNAL_H_
#define GB_CLI_CLI_INTERNAL_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_command_line.h"

namespace gb::internal {

//==============================================================================
// Parts of gb_cli shared by its source files. This is private to gb_cli and its
// tests.
//==============================================================================

// The help flag, "--help" or "-h", which every command has. A program's flags
// can't use its names.
extern const CliFlag<bool> kCliHelpFlag;

// The fields every flag has, whatever its value type.
struct CliFlagFields {
  // The CliFlag<T>, which identifies the flag.
  const void* definition = nullptr;

  // The flag's long name, without the leading "--".
  std::string_view name;

  // The flag's short name, without the leading "-", or 0 if it has none.
  char short_name = 0;

  // Whether the flag may be left out.
  bool optional = true;

  // Whether the flag may be given more than once.
  bool repeated = false;
};

// The fields every positional argument has, whatever its value type.
struct CliArgFields {
  // The CliArg<T>, which identifies the argument.
  const void* definition = nullptr;

  // The argument's name, without the surrounding "<" and ">".
  std::string_view name;

  // Whether the argument may be left out.
  bool optional = false;
};

// Two lists iterated as one: the items of `first`, then those of `second`.
// This holds spans, so the lists must outlive it.
template <typename T>
class CliJoinedList {
 public:
  class Iterator {
   public:
    const T& operator*() const { return (*list_)[index_]; }
    Iterator& operator++() {
      ++index_;
      return *this;
    }
    bool operator==(const Iterator& other) const {
      return index_ == other.index_;
    }

   private:
    friend class CliJoinedList;

    Iterator(const CliJoinedList* list, int index)
        : list_(list), index_(index) {}

    const CliJoinedList* list_;
    int index_;
  };

  CliJoinedList(absl::Span<const T> first, absl::Span<const T> second)
      : first_(first), second_(second) {}

  Iterator begin() const { return Iterator(this, 0); }
  Iterator end() const { return Iterator(this, size()); }
  bool empty() const { return size() == 0; }

 private:
  int size() const { return static_cast<int>(first_.size() + second_.size()); }
  const T& operator[](int index) const {
    const int first_size = static_cast<int>(first_.size());
    return index < first_size ? first_[index] : second_[index - first_size];
  }

  absl::Span<const T> first_;
  absl::Span<const T> second_;
};

// Returns all of a command's flags: its `flags`, then its `added_flags`.
CliJoinedList<CliAnyFlag> GetCliFlags(const CliCommand* command);

// Returns all of a command's subcommands: its `commands`, then its
// `added_commands`.
CliJoinedList<const CliCommand*> GetCliCommands(const CliCommand* command);

CliFlagFields GetCliFlagFields(const CliAnyFlag& flag);
CliArgFields GetCliArgFields(const CliAnyArg& arg);

// Returns the value parsed from `text` and validated, or an error saying what
// is wrong with it. A bool flag takes no value, so `text` is ignored, and its
// value is true. Each validator gets a context made from `context` with only
// its own constraints, so `context` must meet them all (see
// AppendCliValidatorConstraints()).
absl::StatusOr<CliValue> ParseCliValue(ValidatedContext* context,
                                       const CliAnyFlag& flag,
                                       std::string_view text);
absl::StatusOr<CliValue> ParseCliValue(ValidatedContext* context,
                                       const CliAnyArg& arg,
                                       std::string_view text);

// Appends the constraints of a flag's or argument's validators to
// `constraints`.
void AppendCliValidatorConstraints(const CliAnyFlag& flag,
                                   std::vector<ContextConstraint>* constraints);
void AppendCliValidatorConstraints(const CliAnyArg& arg,
                                   std::vector<ContextConstraint>* constraints);

// Returns the names of the commands in `path`, separated by spaces, as used to
// say which command an error is for: "program build".
std::string GetCliPathName(absl::Span<const CliCommand* const> path);

// Appends the arguments of `command` to `usage`, as help's usage line shows
// them: " <input> [<output>]".
void AppendCliArgUsage(const CliCommand* command, std::string* usage);

// Returns how the commands in `path` are written on the command line, with the
// arguments of each command before the last ahead of its subcommand's name, as
// help and the hint for it show: "program <image> format". `path` must not be
// empty.
std::string GetCliUsagePath(absl::Span<const CliCommand* const> path);

}  // namespace gb::internal

#endif  // GB_CLI_CLI_INTERNAL_H_
