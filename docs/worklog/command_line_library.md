# Command line library

A library for a program's command line, `gb_cli`, defined in one place as
constant data rather than by flags declared throughout the code. A program
defines its commands, their flags and positional arguments, and the help for
each, then hands `main`'s arguments to `RunCommandLine()`. That parses them,
validates the values, and runs the function bound to the command they name, or
writes the help text, or writes what is wrong with the command line.

OZ-3 is the first user (*Command lines on Game Bits*): `oz3ism` and `oz3asm`
parse their command lines by hand today, and keep the short flags the wiki
documents (`oz3asm -i <instruction set> -m -l <source file> [<program
file>]`).

## Behavior

```
const gb::CliFlag<std::string_view> kInstructionSet = {
    .name = "instruction-set",
    .short_name = 'i',
    .value_name = "instruction set",
    .help = "Assembles for the instruction set in this source file (.izm), "
            "rather than the default one.",
    .validators = {gb::CliNotEmpty},
};
const gb::CliFlag<bool> kMap = {
    .name = "map",
    .short_name = 'm',
    .help = "Writes the program's map beside the program file (.oz3map).",
};
const gb::CliArg<std::string_view> kSourceFile = {
    .name = "source file",
    .help = "The program to assemble.",
    .validators = {gb::CliNotEmpty},
};
const gb::CliArg<std::string_view> kProgramFile = {
    .name = "program file",
    .help = "Where to write the program. Defaults to the source file's path "
            "with the extension .oz3.",
    .optional = true,
};

bool Run(const gb::CliValues& values);

const gb::CliCommand kProgram = {
    .name = "oz3asm",
    .help = "Assembles a program, and writes its program file.",
    .flags = {&kInstructionSet, &kMap},
    .args = {&kSourceFile, &kProgramFile},
    .run = Run,
};

bool Run(const gb::CliValues& values) {
  std::optional<std::string_view> instruction_set = values.Get(kInstructionSet);
  const bool write_map = values.Get(kMap);
  const std::string_view source_path = *values.Get(kSourceFile);
  ...
}

int main(int argc, char* argv[]) {
  return gb::RunCommandLine(&kProgram, gb::GetMainArgs(argc, argv));
}
```

### Definitions

- **Constant data:** every definition is a `const` aggregate, written with
  designated initializers, with static storage (at namespace scope, or
  `static` in a function). Its lists (`flags`, `args`, `commands`, `names`,
  `validators`) are initializer lists, so a definition holds pointers into
  constant data. It is trivially destructible, so it is safe as a global under
  the Google style rule for static storage. The cost is that a definition must
  not be a temporary or a local copied elsewhere, which the header says.
- **Not constexpr:** MSVC miscompiles a `constexpr` aggregate with an
  initializer list member, leaving the list's elements zero at run time
  (`const` is correct). So each definition with lists holds an
  `internal::CliNotConstexpr` member, whose constructor isn't constexpr, and a
  `constexpr` definition fails to compile instead. Those definitions are then
  initialized dynamically, at startup. That is safe because definitions refer
  to each other only by address, and must not be read before `main()`.
- **Commands:** a `CliCommand` has a name, help, flags, positional arguments,
  subcommands, and an optional run function, `bool (*)(const CliValues&)`.
  The program is the root command, and its name is used in help rather than
  `argv[0]`. A command has either arguments or subcommands, not both. A
  command with subcommands but no run function needs one of them to be named.
- **Flags:** a `CliFlag<T>` has a long name (`--name`) and an optional short
  name (`-n`). A flag's `value_name` is shown in help (`--instruction-set
  <instruction set>`), and defaults to the flag's name. A flag may have a
  `default_value`. A `CliFlag<bool>` takes no value: it is true if it is
  given and false otherwise, so it has no value name, default, or validators.
- **Positional arguments:** a `CliArg<T>` has a name, shown in help as
  `<name>`, and help. Optional arguments (`.optional = true`) follow the
  required ones, and may have a `default_value`.
- **Value types:** `std::string_view`, `int64_t`, `double`, and enumerations,
  and `bool` for flags only. Numbers are only the largest size of each kind, as
  a command line has no use for smaller ones, and the program converts them if
  it needs to. An enumeration's flag or argument lists the names it accepts in
  `names`, each a `CliName<T>` of a name, its value, and optional help. For any
  other type `names` is an empty struct, so names given to it fail to compile.
  Any other value type fails to compile. A string value points into the
  arguments, which must outlive the values.
- **Numbers:** the text must be exactly the number, with no surrounding space:
  a base 10 integer with an optional sign, or a finite floating point number
  (not `inf` or `nan`), in range. Abseil's `SimpleAtoi()` and `SimpleAtod()`
  parse them, with checks for what they allow beyond that (space, and infinity
  for an out of range value).
- **Validators:** a flag or argument has a list of validators,
  `absl::Status (*)(const T& value)`, each returning an error saying what is
  wrong with the value. The common ones are `CliInRange<kMin, kMax>`,
  `CliAtLeast<kMin>`, `CliAtMost<kMax>` (function templates, so the bounds are
  part of the constant), and `CliNotEmpty`. A default value is not validated.
  There is no validator for paths: one would check them against the program's
  `FileSystem`, which a validator (a plain function pointer) has no way to be
  given, so the command checks its paths when it runs.
- **Definition errors:** a definition that can't be parsed unambiguously is a
  programmer error, found by `ValidateCliCommand()`: empty or duplicate names
  (a flag's long and short names are unique across the command and the
  commands above it), names starting with `-`, a short name that is `-`, `.`,
  a digit, or not visible ASCII, `--help` or `-h` (see Help), an
  optional argument before a required one, arguments and subcommands on one
  command, a bool flag with a value name, default, or validators, or an
  enumeration without names or with an empty or duplicate one, or one
  starting with `-` (which a positional argument would parse as a flag). `ParseCommandLine()` CHECKs it, so
  any run of the program finds a bad definition.

### Parsing

```
absl::StatusOr<CliValues> ParseCommandLine(
    const CliCommand* program, absl::Span<const std::string> args);
```

- `args` are the arguments after the program's name.
- Flags: `--name value`, `--name=value`, `-n value`, and for bool flags
  `--name` or `-n`. Short bool flags don't combine (`-ml` is an unknown flag). The value
  after a flag is taken as it is, even if it starts with `-`
  (`--offset -5`).
- A bare word is the next subcommand's name, if the command has subcommands,
  and otherwise its next positional argument. After `--`, every word is a
  positional argument. `-` alone is a positional argument (the usual name for
  standard input), and so is a word starting with `-` then a digit or `.`, so
  that negative numbers can be given (`prog -5`). No short name is a digit or
  `.`, so this never hides a flag.
- A command's flags are accepted anywhere after its name, including after its
  subcommands' names (`prog --verbose build` and `prog build --verbose`), and
  can be read by the command that runs.
- Errors, as an `InvalidArgument` status whose message is ready to show: an
  unknown flag or command, a flag given twice, a missing value, a value that
  isn't of the type (or one of the names), a value a validator rejects, a
  missing required argument, or an extra argument. Each names the command
  path, and says how to get help: `oz3asm: unknown flag "--foo". Run "oz3asm
  --help" for help.`

### Values

```
class CliValues {
 public:
  const CliCommand* GetProgram() const;
  const CliCommand* GetCommand() const;  // The command that runs.
  absl::Span<const CliCommand* const> GetCommandPath() const;  // Root first.
  bool IsHelp() const;  // See Help.

  bool Get(const CliFlag<bool>& flag) const;
  template <typename T> std::optional<T> Get(const CliFlag<T>& flag) const;
  template <typename T> std::optional<T> Get(const CliArg<T>& arg) const;
};
```

- `Get()` returns the given value, else the default, else `std::nullopt`. A
  required argument is always set. A bool flag reads as a plain `bool`, as a
  `std::optional<bool>` would be true in a condition whenever it holds a
  value, even false.
- `Get()` on a flag or argument that isn't in the command path CHECKs, as the
  code reading it was written for a different command.

### Help

- Every command accepts `--help` and `-h`, which stop parsing and request the
  help for the command so far: `CliValues::IsHelp()`. Nothing after them is
  checked.
- `gb::kCliHelpCommand` is a `help` command a program can add to any command's
  subcommands: `prog help build` is `prog build --help`, and `prog help` is
  `prog --help`.
- `FormatCliHelp(const CliValues&)` returns the help text, wrapped to 80
  columns:

```
Usage: oz3asm [<flags>] <source file> [<program file>]

Assembles a program, and writes its program file.

Arguments:
  <source file>   The program to assemble.
  <program file>  Where to write the program. Defaults to the source file's
                  path with the extension .oz3.

Flags:
  -i, --instruction-set <instruction set>
                  Assembles for the instruction set in this source file
                  (.izm), rather than the default one.
  -m, --map       Writes the program's map beside the program file (.oz3map).
  -h, --help      Writes this help.
```

- A command's help paragraphs are separated by blank lines (`"\n\n"`). Its
  subcommands are listed under `Commands:` with the first paragraph of their
  help, and its flags are followed by those of the commands above it, under
  `Flags for <command>:`. An enumeration lists its names, with their help, and
  a default is shown as `(default: <value>)`.

### Running

```
// What a piece of text written by RunCommandLine() is.
enum class CliText {
  kHelp,   // Help that was asked for.
  kError,  // What is wrong with the command line.
};

// Writes help to stdout, and errors to stderr.
void WriteCliText(CliText type, std::string_view text);

int RunCommandLine(
    const CliCommand* program, absl::Span<const std::string> args,
    absl::FunctionRef<void(CliText type, std::string_view text)> write =
        WriteCliText);

// Returns main()'s arguments after the program's name.
std::vector<std::string> GetMainArgs(int argc, char* argv[]);
```

- `RunCommandLine()` parses the command line, then writes the help if it was
  asked for, or the error if the command line isn't valid, or runs the
  command. Text goes only to `write`, so a program can send it elsewhere (a
  log, a window), and tests can collect it. Returns `main`'s exit code: 0 if
  the command succeeded or help was written, 1 if the command returned false,
  and 2 if the command line isn't valid.
- Arguments and definitions are UTF-8, and pass through as bytes. Help wraps
  by code point, not byte. Getting UTF-8 arguments from the operating system
  is up to the caller: `GetMainArgs()` copies `argv` as it is, which on
  Windows is in the ANSI code page (see *UTF-8 command line arguments on
  Windows* in the backlog). It returns strings rather than views of `argv`
  so that it can convert them later without changing its API.
- Arguments are a span of `std::string` rather than `std::string_view`, so
  that `GetMainArgs()`'s result passes straight in (a vector of strings
  doesn't convert to a span of views). String values point into them, so the
  caller keeps them for as long as it uses the values.

## Design

`gb_cli` is a new tier 1 library, depending only on Abseil. It
is a component a program takes on its own, and nothing in tier 0 needs it.

### Names

- `gb_cli`, and a `Cli` prefix on its types and functions: `CliCommand`,
  `CliFlag`, `CliArg`, `CliName`, `CliValues`. "Flag" alone would
  be confused with `gb::Flags<T>`, the enumeration bit set in `gb_base`, and
  "Option" with the `Options` structs throughout Game Bits.
- `ParseCommandLine()`, `RunCommandLine()`, `FormatCliHelp()`,
  `ValidateCliCommand()`, `CliText` and `WriteCliText()`.
- `GetMainArgs()`: the arguments `main()` is given. Not `GetCommandLine...`,
  which is a Windows macro family (`GetCommandLineW()`), and not `CliArgs`,
  which reads as a list of `CliArg` definitions.

### Files

- `cli.h`: the definitions (`CliCommand`, `CliFlag`, `CliArg`, `CliName`) and
  `ValidateCliCommand()`.
- `cli_internal.h`: what the library's source files share, private to
  `gb_cli` and its tests (`GetCliPathName()`, for errors and help).
- `cli_validators.h`: the common validators.
- `cli_values.h`: `CliValues` and `ParseCommandLine()`.
- `cli_help.h`: `kCliHelpCommand` and `FormatCliHelp()`.
- `run_command_line.h`: `CliText`, `WriteCliText()`, `RunCommandLine()`, and
  `GetMainArgs()`. Nothing in the library is platform-specific.

### Type erasure

```
namespace internal {

// One flag (a CliFlag<T>) in a command's list, with the fields every flag
// has, and functions for the parts that depend on its type.
class CliFlagRef {
 public:
  template <typename T>
  constexpr CliFlagRef(const CliFlag<T>* flag);

  const void* GetDefinition() const;
  std::string_view GetName() const;
  char GetShortName() const;
  bool TakesValue() const;
  ...

  // Parses and validates `text`.
  absl::StatusOr<CliValue> Parse(std::string_view text) const;
  std::string FormatDefault() const;  // Empty if there is none.
  ...
};

// A parsed value. A bool flag that is given is true, and an enumeration is
// held as int64_t.
using CliValue = std::variant<bool, std::string_view, int64_t, double>;

}  // namespace internal
```

- `CliArgRef` is the same for arguments. Each holds a pointer to the
  definition and pointers to functions instantiated for its type, so a ref
  only reads the definition when it is used.
- `CliValues` maps each definition's address to its `CliValue`, in an
  `absl::flat_hash_map`. `Get<T>()` looks it up and converts back, so a read
  costs one hash lookup.
- Parsing is a single pass over the arguments, walking down the command tree.
  It costs nothing that matters: it runs once per program.

**Brittleness:**
- A definition must be static data (see Definitions); one built as a local and
  copied out leaves its lists dangling. Definitions are documented as
  constants with static storage, and the `Behavior` example is the pattern.
  `constexpr`, which would silently break on MSVC, fails to compile.
- A flag must be in its command's `flags` to be read. `Get()` CHECKs if it
  isn't, which any test of the command finds.
- The value types are the alternatives of the `CliValue` variant (with
  enumerations held as `int64_t`), and the check that a definition's type is
  supported is derived from it. A new type is added to the variant and given
  a `ParseCliText()` overload, and a missing overload fails to compile.
- A `CliFlag<bool>` has fields it can't use (`value_name`, `default_value`,
  `validators`). `ValidateCliCommand()` rejects them, so any run of the
  program finds one that is set.

### To confirm

- ~~`constexpr` definitions with initializer list members work on MSVC.~~
  CL1 found they compile, but MSVC (19.44) leaves the lists' elements zero at
  run time, for lists of pointers and of refs alike; `const` definitions are
  correct. Definitions are `const`, and `constexpr` is made a compile error
  (see Definitions).
- ~~`&CliInRange<1, 64>` converts to a validator for an `int64_t` and a
  `double`.~~ CL2 confirmed it does, with or without the `&`, and with
  `double` bounds (`CliInRange<0.5, 1.5>`). In a test macro the
  bounds' comma splits the macro's arguments, so tests name the validator
  first.
- Help counts code points with utfcpp (`utf8::distance()`), which `gb_base`
  already uses privately (an include directory, not a new dependency).
  Checked by CL3.

## CLs

### CL1 [x] gb_cli: commands, flags, and arguments

Depends on: nothing.

- New `src/gb/cli` library, added to `src/gb/CMakeLists.txt` and the README's
  tier 1 list.
- `cli.h`: `CliCommand`, `CliFlag<T>` (including `bool`), `CliArg<T>`, the
  internal refs, and `ValidateCliCommand()`, for `std::string_view` values,
  including running a flag's or argument's validators.
- `cli_values.h`: `CliValues` and `ParseCommandLine()`, with the parsing rules
  and errors above, except help.
- Unused, so no visible change.

**Verify**
- Standard checks (see CLAUDE.md).
- Unit tests: each flag form, bool flags, defaults, `--`, `-`, flags inherited
  by subcommands, optional arguments, subcommands with and without run
  functions, validators (written in the test), and each parse error's
  message. `ValidateCliCommand()` for each definition error, and a death test
  that `ParseCommandLine()` CHECKs it, and that `Get()` CHECKs a flag outside
  the command path.
- A test that a `const` definition's lists hold their elements at run time.
- By hand: a `constexpr` definition fails to compile, and so does a
  `CliArg<bool>`.

### CL2 [x] gb_cli: number and enumeration values, and validators

Depends on: CL1.

- `int64_t`, `double`, and enumeration values with `CliName`, parsed with
  Abseil's `SimpleAtoi` and `SimpleAtod`.
- `cli_validators.h`: `CliInRange`, `CliAtLeast`, `CliAtMost`, and
  `CliNotEmpty`.

**Verify**
- Standard checks.
- Unit tests: each type's values, out of range and malformed numbers, unknown
  names, each validator, several validators on one flag, and that a default
  isn't validated.
- By hand: `names` on a flag or argument that isn't an enumeration fails to
  compile.

### CL3 [ ] gb_cli: help

Depends on: CL2.

- `--help` and `-h` on every command, and `CliValues::IsHelp()`.
- `cli_help.h`: `kCliHelpCommand` and `FormatCliHelp()`.

**Verify**
- Standard checks.
- Unit tests: the full help text for a program with arguments, one with
  subcommands and inherited flags, an enumeration flag, defaults, long flag
  names that push help onto the next line, and wrapping. `--help` in each
  position, with invalid arguments after it, and `help <command>`.

### CL4 [ ] gb_cli: running a command line

Depends on: CL3.

- `run_command_line.h`: `CliText`, `WriteCliText()`, `RunCommandLine()`, and
  `GetMainArgs()`.

**Verify**
- Standard checks.
- Unit tests: the exit codes, that the run function gets the values, and the
  text written for help and errors, collected by the test's `write`.
  `GetMainArgs()` skips the program's name, and returns nothing for an
  `argc` of 1. `WriteCliText()` itself (stdout and stderr) is the only part
  left untested.
