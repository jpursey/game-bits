// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_help.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "gb/cli/cli_command_line.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

bool RunNothing(ValidatedContext&, const CliCommandLine&) { return true; }

// Returns the help for the command the command line `args` names, which must
// be valid.
std::string GetHelp(const CliCommand* program,
                    const std::vector<std::string_view>& args) {
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(program, args);
  if (!command_line.ok()) {
    ADD_FAILURE() << command_line.status();
    return "";
  }
  return FormatCliHelp(command_line->GetCommandPath());
}

//==============================================================================
// A program with flags and positional arguments.
//==============================================================================

const CliFlag<std::string_view> kInstructionSet = {
    .name = "instruction-set",
    .short_name = 'i',
    .value_name = "instruction set",
    .help =
        "Assembles for the instruction set in this source file (.izm), "
        "rather than the default one.",
};
const CliFlag<bool> kMap = {
    .name = "map",
    .short_name = 'm',
    .help = "Writes the program's map beside the program file (.oz3map).",
};
const CliArg<std::string_view> kSourceFile = {
    .name = "source file",
    .help = "The program to assemble.",
};
const CliArg<std::string_view> kProgramFile = {
    .name = "program file",
    .help =
        "Where to write the program. Defaults to the source file's path "
        "with the extension .oz3.",
    .optional = true,
};
const CliCommand kAssembler = {
    .name = "oz3asm",
    .help = "Assembles a program, and writes its program file.",
    .flags = {&kInstructionSet, &kMap},
    .args = {&kSourceFile, &kProgramFile},
    .run = RunNothing,
};

TEST(FormatCliHelpTest, Arguments) {
  EXPECT_EQ(
      GetHelp(&kAssembler, {"--help"}),
      "Usage: oz3asm [<flags>] <source file> [<program file>]\n"
      "\n"
      "Assembles a program, and writes its program file.\n"
      "\n"
      "Arguments:\n"
      "  <source file>   The program to assemble.\n"
      "  <program file>  Where to write the program. Defaults to the source "
      "file's path\n"
      "                  with the extension .oz3.\n"
      "\n"
      "Flags:\n"
      "  -i, --instruction-set <instruction set>\n"
      "                  Assembles for the instruction set in this source "
      "file (.izm),\n"
      "                  rather than the default one.\n"
      "  -m, --map       Writes the program's map beside the program file "
      "(.oz3map).\n"
      "  -h, --help      Writes help for this command.\n");
}

//==============================================================================
// A program with subcommands.
//==============================================================================

const CliFlag<bool> kVerbose = {
    .name = "verbose",
    .short_name = 'v',
    .help = "Writes more.",
};
const CliFlag<bool> kForce = {
    .name = "force",
    .short_name = 'f',
    .help = "Replaces a remote with the same name.",
};
const CliArg<std::string_view> kName = {
    .name = "name",
    .help = "The remote's name.",
};
const CliCommand kAdd = {
    .name = "add",
    .help = "Adds a remote.",
    .flags = {&kForce},
    .args = {&kName},
    .run = RunNothing,
};
const CliCommand kRemote = {
    .name = "remote",
    .help = "Manages remotes.\n\nOnly the first paragraph is listed.",
    .commands = {&kAdd},
};
const CliCommand kStatus = {.name = "status", .run = RunNothing};
const CliCommand kTool = {
    .name = "tool",
    .help = "Manages things.\n\nIn more than one paragraph.",
    .flags = {&kVerbose},
    .commands = {&kRemote, &kStatus, &kCliHelpCommand},
    .run = RunNothing,
};

TEST(FormatCliHelpTest, Commands) {
  EXPECT_EQ(GetHelp(&kTool, {"--help"}),
            "Usage: tool [<flags>] [<command>]\n"
            "\n"
            "Manages things.\n"
            "\n"
            "In more than one paragraph.\n"
            "\n"
            "Commands:\n"
            "  remote         Manages remotes.\n"
            "  status\n"
            "  help           Writes help for a command.\n"
            "\n"
            "Flags:\n"
            "  -v, --verbose  Writes more.\n"
            "  -h, --help     Writes help for this command.\n");
}

TEST(FormatCliHelpTest, CommandsRequired) {
  EXPECT_EQ(GetHelp(&kTool, {"remote", "--help"}),
            "Usage: tool remote [<flags>] <command>\n"
            "\n"
            "Manages remotes.\n"
            "\n"
            "Only the first paragraph is listed.\n"
            "\n"
            "Commands:\n"
            "  add            Adds a remote.\n"
            "\n"
            "Flags:\n"
            "  -h, --help     Writes help for this command.\n"
            "\n"
            "Flags for tool:\n"
            "  -v, --verbose  Writes more.\n");
}

TEST(FormatCliHelpTest, InheritedFlags) {
  EXPECT_EQ(GetHelp(&kTool, {"remote", "add", "-h"}),
            "Usage: tool remote add [<flags>] <name>\n"
            "\n"
            "Adds a remote.\n"
            "\n"
            "Arguments:\n"
            "  <name>         The remote's name.\n"
            "\n"
            "Flags:\n"
            "  -f, --force    Replaces a remote with the same name.\n"
            "  -h, --help     Writes help for this command.\n"
            "\n"
            "Flags for tool:\n"
            "  -v, --verbose  Writes more.\n");
}

TEST(FormatCliHelpTest, HelpCommand) {
  EXPECT_EQ(GetHelp(&kTool, {"help", "remote", "add"}),
            GetHelp(&kTool, {"remote", "add", "--help"}));
  EXPECT_EQ(GetHelp(&kTool, {"help"}), GetHelp(&kTool, {"--help"}));
  EXPECT_EQ(GetHelp(&kTool, {"help", "help"}),
            "Usage: tool help [<flags>] [<command>]\n"
            "\n"
            "Writes help for a command.\n"
            "\n"
            "Arguments:\n"
            "  <command>      The command to write help for, then the names of "
            "any of its\n"
            "                 subcommands. If none is given, this writes help "
            "for the command\n"
            "                 \"help\" belongs to.\n"
            "\n"
            "Flags:\n"
            "  -h, --help     Writes help for this command.\n"
            "\n"
            "Flags for tool:\n"
            "  -v, --verbose  Writes more.\n");
}

//==============================================================================
// A program with choices and defaults.
//==============================================================================

const CliFlag<std::string_view> kShape = {
    .name = "shape",
    .help = "The shape to draw.",
    .choices = {{.name = "circle", .help = "A round shape."},
                {.name = "square"}},
    .default_value = "circle",
};
const CliFlag<int64_t> kCount = {
    .name = "count",
    .value_name = "n",
    .default_value = 3,
};
const CliFlag<double> kScale = {
    .name = "scale",
    .help = "How much to scale it.",
    .default_value = 1.5,
};
const CliFlag<std::string_view> kTitle = {.name = "title", .default_value = ""};
const CliArg<std::string_view> kMode = {
    .name = "mode",
    .choices = {{.name = "fill"}, {.name = "line"}},
    .default_value = "unused",
};
const CliArg<std::string_view> kOutput = {
    .name = "output",
    .optional = true,
    .default_value = "out.svg",
};
const CliCommand kDraw = {
    .name = "draw",
    .flags = {&kShape, &kCount, &kScale, &kTitle},
    .args = {&kMode, &kOutput},
    .run = RunNothing,
};

TEST(FormatCliHelpTest, ChoicesAndDefaults) {
  // A required argument's default is never used, so it isn't shown.
  EXPECT_EQ(GetHelp(&kDraw, {"--help"}),
            "Usage: draw [<flags>] <mode> [<output>]\n"
            "\n"
            "Arguments:\n"
            "  <mode>               fill\n"
            "                       line\n"
            "  <output>             (default: out.svg)\n"
            "\n"
            "Flags:\n"
            "      --shape <shape>  The shape to draw. (default: circle)\n"
            "                       circle  A round shape.\n"
            "                       square\n"
            "      --count <n>      (default: 3)\n"
            "      --scale <scale>  How much to scale it. (default: 1.5)\n"
            "      --title <title>  (default: \"\")\n"
            "  -h, --help           Writes help for this command.\n");
}

TEST(FormatCliHelpTest, FloatingPointDefaultsAreExact) {
  static const CliFlag<double> kA = {.name = "a", .default_value = 0.1234567};
  static const CliFlag<double> kB = {.name = "b", .default_value = 1234567.0};
  static const CliFlag<double> kC = {.name = "c", .default_value = 0.1};
  static const CliCommand kProgram = {.name = "prog", .flags = {&kA, &kB, &kC}};
  EXPECT_EQ(GetHelp(&kProgram, {"--help"}),
            "Usage: prog [<flags>]\n"
            "\n"
            "Flags:\n"
            "      --a <a>  (default: 0.1234567)\n"
            "      --b <b>  (default: 1234567)\n"
            "      --c <c>  (default: 0.1)\n"
            "  -h, --help   Writes help for this command.\n");
}

//==============================================================================
// Wrapping
//==============================================================================

TEST(FormatCliHelpTest, WrapsByCodePoint) {
  // Each word is 5 code points ("\xC3\xA9" is e with an acute accent), but 10
  // bytes, so 13 words fit in 80 columns.
  const std::string word = "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9";
  static const std::string help =
      absl::StrJoin(std::vector<std::string>(16, word), " ");
  static const CliCommand kProgram = {.name = "prog", .help = help};
  EXPECT_EQ(
      GetHelp(&kProgram, {"--help"}),
      absl::StrCat("Usage: prog [<flags>]\n"
                   "\n",
                   absl::StrJoin(std::vector<std::string>(13, word), " "), "\n",
                   absl::StrJoin(std::vector<std::string>(3, word), " "),
                   "\n"
                   "\n"
                   "Flags:\n"
                   "  -h, --help  Writes help for this command.\n"));
}

TEST(FormatCliHelpTest, LongWordIsNotSplit) {
  const std::string long_word(90, 'x');
  static const std::string help = absl::StrCat("Short ", long_word, " short.");
  static const CliCommand kProgram = {.name = "prog", .help = help};
  EXPECT_EQ(GetHelp(&kProgram, {"--help"}),
            absl::StrCat("Usage: prog [<flags>]\n"
                         "\n"
                         "Short\n",
                         long_word,
                         "\n"
                         "short.\n"
                         "\n"
                         "Flags:\n"
                         "  -h, --help  Writes help for this command.\n"));
}

TEST(FormatCliHelpTest, EntryColumnUsesCodePoints) {
  static const CliArg<std::string_view> kCafe = {
      .name = "caf\xC3\xA9",
      .help = "Where.",
  };
  static const CliCommand kProgram = {.name = "prog", .args = {&kCafe}};
  EXPECT_EQ(GetHelp(&kProgram, {"--help"}),
            "Usage: prog [<flags>] <caf\xC3\xA9>\n"
            "\n"
            "Arguments:\n"
            "  <caf\xC3\xA9>      Where.\n"
            "\n"
            "Flags:\n"
            "  -h, --help  Writes help for this command.\n");
}

}  // namespace
}  // namespace gb
