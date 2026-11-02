// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_help.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/types/span.h"
#include "gb/cli/cli_internal.h"

namespace gb {

namespace {

const CliArg<std::string_view> kHelpCommandArg = {
    .name = "command",
    .help =
        "The command to write help for, then the names of any of its "
        "subcommands. If none is given, this writes help for the command "
        "\"help\" belongs to.",
    .optional = true,
};

}  // namespace

const CliCommand kCliHelpCommand = {
    .name = "help",
    .help = "Writes help for a command.",
    .args = {&kHelpCommandArg},
};

namespace {

// The indent of each entry in a list of arguments, commands, or flags.
constexpr int kIndent = 2;

// The widest entry that has its help beside it. A wider one has its help start
// on the next line.
constexpr int kMaxEntryWidth = 26;

// The columns between an entry and its help.
constexpr int kGap = 2;

// Returns the width of `text` in columns, counting each UTF-8 code point as
// one.
int GetWidth(std::string_view text) {
  // Every code point has one byte that isn't a continuation byte (10xxxxxx).
  return static_cast<int>(absl::c_count_if(text, [](char c) {
    return (static_cast<unsigned char>(c) & 0xC0) != 0x80;
  }));
}

std::vector<std::string> GetParagraphs(std::string_view help) {
  return absl::StrSplit(help, "\n\n", absl::SkipWhitespace());
}

//==============================================================================
// Entries
//==============================================================================

// One entry in a list of arguments, commands, or flags.
struct Entry {
  // The entry as it is written on the command line: "-o, --output <file>".
  std::string name;

  // The entry's help paragraphs.
  std::vector<std::string> paragraphs;

  // The values the entry accepts, if it lists them.
  std::initializer_list<CliChoice> choices;
};

// A titled list of entries.
struct Section {
  std::string title;
  std::vector<Entry> entries;
};

std::string FormatDefault(std::string_view value) {
  return value.empty() ? "\"\"" : std::string(value);
}

std::string FormatDefault(int64_t value) { return absl::StrCat(value); }

std::string FormatDefault(double value) {
  // The shortest text that parses back to `value`, unlike absl::StrCat(),
  // which rounds to six digits. No double needs more than 24 characters.
  char text[32];
  const std::to_chars_result result =
      std::to_chars(text, text + sizeof(text), value);
  return std::string(text, result.ptr);
}

// Adds `default_value` to the end of the help, if there is one.
template <typename T>
void AddDefault(const std::optional<T>& default_value,
                std::vector<std::string>* paragraphs) {
  if (!default_value.has_value()) {
    return;
  }
  const std::string text =
      absl::StrCat("(default: ", FormatDefault(*default_value), ")");
  if (paragraphs->empty()) {
    paragraphs->push_back(text);
  } else {
    absl::StrAppend(&paragraphs->back(), " ", text);
  }
}

template <typename T>
Entry GetEntry(const CliFlag<T>& flag) {
  Entry entry = {
      .paragraphs = GetParagraphs(flag.help),
      .choices = flag.choices,
  };
  if (flag.short_name != 0) {
    entry.name = absl::StrCat("-", std::string_view(&flag.short_name, 1), ", ");
  } else {
    entry.name = "    ";
  }
  absl::StrAppend(&entry.name, "--", flag.name);
  if constexpr (!std::is_same_v<T, bool>) {
    absl::StrAppend(&entry.name, " <",
                    flag.value_name.empty() ? flag.name : flag.value_name, ">");
    AddDefault(flag.default_value, &entry.paragraphs);
  }
  return entry;
}

template <typename T>
Entry GetEntry(const CliArg<T>& arg) {
  Entry entry = {
      .name = absl::StrCat("<", arg.name, ">"),
      .paragraphs = GetParagraphs(arg.help),
      .choices = arg.choices,
  };

  // A required argument's default is never used.
  if (arg.optional) {
    AddDefault(arg.default_value, &entry.paragraphs);
  }
  return entry;
}

Entry GetEntry(const CliCommand* command) {
  std::vector<std::string> paragraphs = GetParagraphs(command->help);
  if (paragraphs.size() > 1) {
    paragraphs.resize(1);
  }
  return {.name = std::string(command->name),
          .paragraphs = std::move(paragraphs)};
}

std::vector<Entry> GetEntries(std::initializer_list<CliAnyFlag> flags) {
  std::vector<Entry> entries;
  for (const CliAnyFlag& flag : flags) {
    entries.push_back(
        std::visit([](const auto* typed) { return GetEntry(*typed); }, flag));
  }
  return entries;
}

std::vector<Entry> GetEntries(std::initializer_list<CliAnyArg> args) {
  std::vector<Entry> entries;
  for (const CliAnyArg& arg : args) {
    entries.push_back(
        std::visit([](const auto* typed) { return GetEntry(*typed); }, arg));
  }
  return entries;
}

std::vector<Entry> GetEntries(
    std::initializer_list<const CliCommand*> commands) {
  std::vector<Entry> entries;
  for (const CliCommand* command : commands) {
    entries.push_back(GetEntry(command));
  }
  return entries;
}

//==============================================================================
// HelpWriter
//==============================================================================

// Writes help text, keeping track of the column it is at.
class HelpWriter {
 public:
  // Wraps lines to `width` columns.
  explicit HelpWriter(int width) : width_(width) {}

  std::string Take() { return std::move(text_); }

  // Writes `text`, which must not contain a newline.
  void Write(std::string_view text) { Write(text, GetWidth(text)); }

  // Ends the line.
  void NewLine();

  // Writes `text` a word at a time from `column` (see MoveTo()), wrapping
  // lines to the width, with each new line also starting at `column`.
  void WriteWrapped(std::string_view text, int column);

  // Writes an entry, with its help starting at `column`.
  void WriteEntry(const Entry& entry, int column);

 private:
  // Writes `text`, which is `width` columns wide.
  void Write(std::string_view text, int width);

  // Moves to `column`: on this line if it is at line start, or at least kGap
  // columns past what is written, and otherwise on the next line.
  void MoveTo(int column);

  const int width_;
  std::string text_;
  int column_ = 0;
};

void HelpWriter::NewLine() {
  text_.push_back('\n');
  column_ = 0;
}

void HelpWriter::WriteWrapped(std::string_view text, int column) {
  MoveTo(column);
  bool first = true;
  for (const std::string_view word :
       absl::StrSplit(text, absl::ByAsciiWhitespace(), absl::SkipEmpty())) {
    const int width = GetWidth(word);
    if (first) {
      first = false;
    } else if (column_ + 1 + width > width_) {
      NewLine();
      MoveTo(column);
    } else {
      Write(" ", 1);
    }
    Write(word, width);
  }
}

void HelpWriter::WriteEntry(const Entry& entry, int column) {
  MoveTo(kIndent);
  Write(entry.name);
  for (const std::string& paragraph : entry.paragraphs) {
    WriteWrapped(paragraph, column);
  }
  int choice_width = 0;
  for (const CliChoice& choice : entry.choices) {
    choice_width = std::max(choice_width, GetWidth(choice.name));
  }
  const int choice_help_column = column + choice_width + kGap;
  for (const CliChoice& choice : entry.choices) {
    MoveTo(column);
    Write(choice.name);
    if (!choice.help.empty()) {
      WriteWrapped(choice.help, choice_help_column);
    }
  }
  NewLine();
}

void HelpWriter::Write(std::string_view text, int width) {
  text_.append(text);
  column_ += width;
}

void HelpWriter::MoveTo(int column) {
  if (column_ != 0 && column_ + kGap > column) {
    NewLine();
  }
  text_.append(column - column_, ' ');
  column_ = column;
}

}  // namespace

//==============================================================================
// FormatCliHelp
//==============================================================================

std::string FormatCliHelp(absl::Span<const CliCommand* const> path, int width) {
  const CliCommand* command = path.back();

  std::string usage =
      absl::StrCat("Usage: ", internal::GetCliPathName(path), " [<flags>]");
  for (const CliAnyArg& arg : command->args) {
    const internal::CliArgFields fields = internal::GetCliArgFields(arg);
    if (fields.optional) {
      absl::StrAppend(&usage, " [<", fields.name, ">]");
    } else {
      absl::StrAppend(&usage, " <", fields.name, ">");
    }
  }
  if (command->commands.size() != 0) {
    absl::StrAppend(&usage,
                    command->run != nullptr ? " [<command>]" : " <command>");
  }

  std::vector<Section> sections;
  sections.push_back(
      {.title = "Arguments:", .entries = GetEntries(command->args)});
  sections.push_back(
      {.title = "Commands:", .entries = GetEntries(command->commands)});
  sections.push_back(
      {.title = "Flags:", .entries = GetEntries(command->flags)});
  sections.back().entries.push_back(GetEntry(internal::kCliHelpFlag));
  for (int i = static_cast<int>(path.size()) - 2; i >= 0; --i) {
    sections.push_back({
        .title = absl::StrCat("Flags for ",
                              internal::GetCliPathName(path.first(i + 1)), ":"),
        .entries = GetEntries(path[i]->flags),
    });
  }

  int max_entry_width = 0;
  for (const Section& section : sections) {
    for (const Entry& entry : section.entries) {
      const int width = GetWidth(entry.name);
      if (width <= kMaxEntryWidth) {
        max_entry_width = std::max(max_entry_width, width);
      }
    }
  }
  const int column = kIndent + max_entry_width + kGap;

  HelpWriter writer(width);
  writer.Write(usage);
  writer.NewLine();
  for (const std::string& paragraph : GetParagraphs(command->help)) {
    writer.NewLine();
    writer.WriteWrapped(paragraph, 0);
    writer.NewLine();
  }
  for (const Section& section : sections) {
    if (section.entries.empty()) {
      continue;
    }
    writer.NewLine();
    writer.Write(section.title);
    writer.NewLine();
    for (const Entry& entry : section.entries) {
      writer.WriteEntry(entry, column);
    }
  }
  return writer.Take();
}

}  // namespace gb
