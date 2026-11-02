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
    .validators = {&gb::kCliNotEmpty},
};
const gb::CliFlag<bool> kMap = {
    .name = "map",
    .short_name = 'm',
    .help = "Writes the program's map beside the program file (.oz3map).",
};
const gb::CliArg<std::string_view> kSourceFile = {
    .name = "source file",
    .help = "The program to assemble.",
    .validators = {&gb::kCliNotEmpty},
};
const gb::CliArg<std::string_view> kProgramFile = {
    .name = "program file",
    .help = "Where to write the program. Defaults to the source file's path "
            "with the extension .oz3.",
    .optional = true,
};

bool Run(gb::ValidatedContext& context,
         const gb::CliCommandLine& command_line);

const gb::CliCommand kProgram = {
    .name = "oz3asm",
    .help = "Assembles a program, and writes its program file.",
    .flags = {&kInstructionSet, &kMap},
    .args = {&kSourceFile, &kProgramFile},
    .run = Run,
};

bool Run(gb::ValidatedContext& context,
         const gb::CliCommandLine& command_line) {
  std::optional<std::string_view> instruction_set =
      command_line.Get(kInstructionSet);
  const bool write_map = command_line.Get(kMap);
  const std::string_view source_path = *command_line.Get(kSourceFile);
  ...
}

int main(int argc, char* argv[]) {
  return gb::RunCommandLine(&kProgram, gb::GetMainArgs(argc, argv));
}
```

### Definitions

- **Constant data:** every definition is a `const` aggregate, written with
  designated initializers, with static storage (at namespace scope, or
  `static` in a function). Its lists (`flags`, `args`, `commands`, `choices`,
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
  subcommands, and an optional run function, `bool (*)(const CliCommandLine&)`.
  The program is the root command, and its name is used in help rather than
  `argv[0]`. A command has either arguments or subcommands, not both. A
  command with no run function writes its help when it is run, so a command
  with subcommands but no run function writes its help when none is named.
- **Flags:** a `CliFlag<T>` has a long name (`--name`) and an optional short
  name (`-n`). A flag's `value_name` is shown in help (`--instruction-set
  <instruction set>`), and defaults to the flag's name. A flag may have a
  `default_value`. A `CliFlag<bool>` takes no value: it is true if it is
  given and false otherwise, so it has no value name, default, or validators.
- **Positional arguments:** a `CliArg<T>` has a name, shown in help as
  `<name>`, and help. Optional arguments (`.optional = true`) follow the
  required ones, and may have a `default_value`.
- **Value types:** `std::string_view`, `int64_t`, and `double`, and `bool` for
  flags only. Any other value type fails to compile. Numbers are only the
  largest size of each kind, as a command line has no use for smaller ones,
  and the program converts them if it needs to. A string value points into the
  arguments, which must outlive the values.
- **Choices:** a string flag or argument that accepts only some values lists
  them in `choices`, each a `CliChoice` of a name and optional help. There are
  no enumeration values: the program maps the chosen string to its own enum if
  it has one. Choices on any other type are a definition error.
- **Numbers:** the text must be exactly the number, with no surrounding space:
  a base 10 integer with an optional sign, or a finite floating point number
  (not `inf` or `nan`), in range. Abseil's `SimpleAtoi()` and `SimpleAtod()`
  parse them, with checks for what they allow beyond that (space, and infinity
  for an out of range value).
- **Validators:** a flag or argument lists its validators by address, each a
  `CliValidator<T>` definition: a function, `absl::Status (*)(const
  ValidatedContext& context, const T& value)`, returning an error saying what
  is wrong with the value, and the constraints of what it reads from the
  context. The common ones are `kCliInRange<T, kMin, kMax>`,
  `kCliAtLeast<T, kMin>`, `kCliAtMost<T, kMax>` (variable templates, so the
  bounds are part of the constant), and `kCliNotEmpty`: `.validators =
  {&gb::kCliInRange<int64_t, 1, 64>}`. A default value is not validated.
  When the parser reaches a command, the context is validated against the
  constraints of the validators of its flags and arguments, and those of the
  commands above it, whether or not they are given. A value that is missing is
  reported as `ValidatedContext` reports any error, and `ParseCommandLine()`
  returns a `FailedPrecondition` error, as that is the program's mistake, not
  the user's.
- **Definition errors:** a definition that can't be parsed unambiguously is a
  programmer error, found by `ValidateCliCommand()`: empty or duplicate names
  (a flag's long and short names are unique across the command and the
  commands above it), names starting with `-`, a short name that is `-`, `.`,
  a digit, or not visible ASCII, `--help` or `-h` (see Help), an
  optional argument before a required one, arguments and subcommands on one
  command, a bool flag with a value name, default, or validators, choices on a
  type other than a string, or a choice that is empty, a duplicate, or starts
  with `-` (which a positional argument would parse as a flag), a null
  constraint on a command, and a null validator, one with no function, or one
  with a constraint that is null or isn't an input (`kInRequired` or
  `kInOptional`).
  `ParseCommandLine()` CHECKs it, so any run of the program finds a bad
  definition.

### Parsing

```
absl::StatusOr<CliCommandLine> ParseCommandLine(
    const CliCommand* program, absl::Span<const std::string_view> args);
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
  isn't of the type (or one of the choices), a value a validator rejects, a
  missing required argument, or an extra argument. Each names the command
  path, and says how to get help: `oz3asm: unknown flag "--foo". Run "oz3asm
  --help" for help.`

### Values

```
class CliCommandLine {
 public:
  const CliCommand* GetProgram() const;
  const CliCommand* GetCommand() const;  // The command that runs.
  absl::Span<const CliCommand* const> GetCommandPath() const;  // Root first.

  // Runs the command, or writes its help (see Help).
  bool Run(absl::FunctionRef<void(std::string_view help)> write_help) const;

  bool Get(const CliFlag<bool>& flag) const;
  template <typename T> std::optional<T> Get(const CliFlag<T>& flag) const;
  template <typename T> std::optional<T> Get(const CliArg<T>& arg) const;
};
```

- `Run()` calls the command's run function with the values, and returns its
  result. If the command line asks for help, or the command has no run
  function, it writes the command's help to `write_help` instead, and returns
  true. So a run function never sees the partial values of a request for help,
  and a caller handles help and running the same way.
- `Get()` returns the given value, else the default, else `std::nullopt`. A
  required argument is always set. A bool flag reads as a plain `bool`, as a
  `std::optional<bool>` would be true in a condition whenever it holds a
  value, even false.
- `Get()` on a flag or argument that isn't in the command path CHECKs, as the
  code reading it was written for a different command. So does `Get()` on the
  values of a request for help, which may be missing values, including
  required arguments.

### Help

- Every command accepts `--help` and `-h`, which stop parsing and request the
  help for the command so far, which `CliCommandLine::Run()` writes. Nothing after
  them is checked.
- `gb::kCliHelpCommand` is a `help` command a program can add to any command's
  subcommands: `prog help build` is `prog build --help`, and `prog help` is
  `prog --help`. Every word after `help` names a subcommand, so the help
  command isn't in the command path, unless it is named again (`prog help
  help`). It has an optional `<command>` argument, which only its own help
  shows.
- A program's own flags can't be `--help` or `-h` (a definition error).
- `FormatCliHelp(absl::Span<const CliCommand* const> path, int width)` returns
  the help text for the command at the end of the command path, wrapped to
  `width` columns. `CliCommandLine::Run()` wraps to the context's width (see
  Context), which is 80 by default:

```
Usage: oz3asm [<flags>] <source file> [<program file>]

Assembles a program, and writes its program file.

Arguments:
  <source file>   The program to assemble.
  <program file>  Where to write the program. Defaults to the source file's path
                  with the extension .oz3.

Flags:
  -i, --instruction-set <instruction set>
                  Assembles for the instruction set in this source file (.izm),
                  rather than the default one.
  -m, --map       Writes the program's map beside the program file (.oz3map).
  -h, --help      Writes help for this command.
```

- A command's help paragraphs are separated by blank lines (`"\n\n"`). Its
  subcommands are listed under `Commands:` with the first paragraph of their
  help, and its flags are followed by those of the commands above it, nearest
  first, under `Flags for <command>:`. A flag or argument with choices lists
  them, with their help, and a default is shown as `(default: <value>)` (an
  empty string as `""`), except for a required argument, which never uses it.
- Help is beside each entry, in a column shared by all the lists. An entry
  wider than 26 columns has its help start on the next line. Lines are
  wrapped at words, up to the width; a word longer than a line isn't split.
  The usage line isn't wrapped. Any width works: a narrow one puts a word on
  each line, still starting at the help column.

### Context

A command line is parsed and run with a `gb::Context` (see
`gb/base/validated_context.h`), which holds what `gb_cli` and the program's
commands need from outside: where text goes, and whatever the program puts
there (a `gb::FileSystem`, its own settings).

```
// cli_writer.h

// What a piece of text written to a CliWriter is.
enum class CliText {
  kHelp,   // Help that was asked for.
  kError,  // What is wrong with a command line.
};

// Writes help to stdout, and errors to stderr. This is the default CliWriter.
void WriteCliText(CliText type, std::string_view text);

// Where a command line's text goes.
using CliWriter = std::function<void(CliText type, std::string_view text)>;

// cli_command_line.h

// The writer for a command line's help and errors. If the context has none,
// WriteCliText() is added to it.
GB_CONTEXT_CONSTRAINT_DEFAULT(kConstraintCliWriter, kInOptional, CliWriter,
                              CliWriter(WriteCliText));

// The width help is wrapped to, in columns. If the context has none,
// kDefaultCliHelpWidth is added to it. A program sets its own with
// `context.SetValue<int>(kKeyCliHelpWidth, width)`.
inline constexpr const char* kKeyCliHelpWidth = "cli_help_width";
inline constexpr int kDefaultCliHelpWidth = 80;
GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintCliHelpWidth, kInOptional, int,
                                    kKeyCliHelpWidth, kDefaultCliHelpWidth);

// What gb_cli reads from a command line's context.
using CliContract =
    ContextContract<kConstraintCliWriter, kConstraintCliHelpWidth>;

absl::StatusOr<CliCommandLine> ParseCommandLine(
    CliContract contract, const CliCommand* program,
    absl::Span<const std::string_view> args);

// As above, with an empty context.
absl::StatusOr<CliCommandLine> ParseCommandLine(
    const CliCommand* program, absl::Span<const std::string_view> args);

class CliCommandLine {
 public:
  ...
  // Runs the command, or writes its help to the context's writer.
  bool Run();
};

// cli.h

struct CliCommand {
  ...
  // The values this command's run function needs from the context, and
  // those it adds. Those of the commands above it apply too.
  std::initializer_list<const ContextConstraint*> constraints;

  // Runs the command. `context` is validated against `gb_cli`'s constraints
  // and those of every command in the path.
  bool (*run)(ValidatedContext& context, const CliCommandLine& command_line);
};
```

- **Parsing:** `ParseCommandLine()` takes the context as a `CliContract`, the
  same way `gb::Game::Run()` takes a `GameContract`: a `Context*`, or an owned
  `Context`. The parsed `CliCommandLine` keeps it, so it becomes move-only.
  Validators read from it while parsing (see Validators), each through a
  `ValidatedContext` with the constraints of the validators of the command
  path so far. A context that fails `gb_cli`'s own constraints (a null
  `Context*`, or a width that isn't an `int`) is a `FailedPrecondition` error
  too.
- **Validator constraints:** a validator is a `CliValidator<T>` definition of
  its function and constraints, rather than a function pointer, so that it
  can say what it reads, as a command does. Every validator takes the
  context, so there is one kind.
- **Validators by address:** a flag or argument lists its validators by
  address, as a command lists its flags, rather than holding copies.
  Definitions are initialized when the program starts, and a copy of a
  validator defined in another file could be made before that file's
  definitions are initialized (the static initialization order problem),
  leaving an empty validator. An address is fixed before any initialization,
  and validators are only read once parsing starts, so nothing in a
  definition depends on the order. The cost is that every validator is a
  named definition, even one used once, and a number validator names its
  value type (`kCliInRange<int64_t, 1, 64>`), as a variable template can't
  take it from the list it is in. A wrong type is a compile error.
- **Running:** `CliCommandLine::Run()` takes no writer. It writes help to the
  context's `CliWriter`. To run
  the command, it validates the context against the constraints of every
  command in the path, and passes that `ValidatedContext` to the run
  function. A run function reads the program's values from it
  (`context.GetPtr<FileSystem>()`), and any `kOut` values it adds are
  validated when it returns. A required value that is missing is reported as
  `ValidatedContext` reports any error (`LOG(FATAL)` in debug builds), and
  `Run()` returns false without running the command. A value the command must
  add but doesn't is reported the same way, and `Run()` returns false.
- **Writing:** `CliWriter` is a single function, given what the text is and
  the text. `kConstraintCliWriter`'s default, `WriteCliText()`, is added to a
  context that has no writer when it is validated, so the context always has
  one: a command can write its own errors with it, where `gb_cli`'s go. The
  default can't be an interface (a default is a copyable value, in a
  `std::any`), so the writer is a `std::function` rather than a class to
  derive from. That also leaves no derived type for `SetPtr(&writer)` to
  store it under by mistake.
- **Help width:** `kConstraintCliHelpWidth` is an `int` named
  `"cli_help_width"`, as `Game`'s `kConstraintMaxFps` is, so a program sets it
  with `context.SetValue<int>(kKeyCliHelpWidth, 100)`. The `cli_` prefix
  keeps it apart from other libraries' values in a shared context.
  `FormatCliHelp()` takes the width rather than a default, so the default is
  only in the constraint.
- **Constraints as data:** a command lists pointers to the
  `ContextConstraint`s it needs, which the `GB_CONTEXT_CONSTRAINT` macros
  define as `inline const` globals, so a command stays constant data:
  `.constraints = {&kConstraintFileSystem}`.

### Running

```
// run_command_line.h

// Parses and runs a command line, returning main()'s exit code.
int RunCommandLine(CliContract contract, const CliCommand* program,
                   absl::Span<const std::string_view> args);

// As above, with an empty context.
int RunCommandLine(const CliCommand* program,
                   absl::Span<const std::string_view> args);

// Returns main()'s arguments after the program's name.
std::vector<std::string_view> GetMainArgs(int argc, char* argv[]);

int main(int argc, char* argv[]) {
  return gb::RunCommandLine(&kProgram, gb::GetMainArgs(argc, argv));
}
```

- `RunCommandLine()` parses the command line, writes the error to the
  context's writer if it isn't valid, and otherwise calls
  `CliCommandLine::Run()`, which writes the help if it was asked for, or runs
  the command. Text goes only to the writer, so a program can send it
  elsewhere (a log, a window), and tests can collect it. Returns `main`'s exit
  code: 0 if the command succeeded or help was written, 1 if the command
  returned false or the context is missing something (the program's mistake),
  and 2 if the command line isn't valid. A context that fails `gb_cli`'s own
  constraints may have no writer, so nothing is written for it beyond what
  `ValidatedContext` reports.
- A parse error says how to get help for the command so far, as part of the
  error's message: `tool remote: unknown flag "--x". Run "tool remote
  --help" for help.` So a program calling `ParseCommandLine()` itself gets
  the same message.
- Arguments and definitions are UTF-8, and pass through as bytes. Help wraps
  by code point, not byte. Getting UTF-8 arguments from the operating system
  is up to the caller: `GetMainArgs()` returns `argv` as it is, which on
  Windows is in the ANSI code page (see *UTF-8 command line arguments on
  Windows* in the backlog).
- Arguments are a span of `std::string_view`, so a caller can pass views of
  whatever holds them. String values point into them, so the caller keeps
  them for as long as it uses the values. `GetMainArgs()` returns views that
  stay valid for the rest of the program: views of `argv` now, and, if it
  converts to UTF-8 later, of converted strings it keeps for the program's
  lifetime, without changing its API.

## Design

`gb_cli` is a new tier 1 library, depending on Abseil, and from CL6 on
`gb_base` for `gb::Context`. It is a component a program takes on its own,
and nothing in tier 0 needs it.

### Names

- `gb_cli`, and a `Cli` prefix on its types and functions: `CliCommand`,
  `CliFlag`, `CliArg`, `CliChoice`, `CliCommandLine`. "Flag" alone would
  be confused with `gb::Flags<T>`, the enumeration bit set in `gb_base`, and
  "Option" with the `Options` structs throughout Game Bits.
- `CliCommandLine`: the parsed command line, which `ParseCommandLine()`
  returns. It holds more than values (the command path, whether help was asked
  for, and `Run()`), so not `CliValues`. Not `CliResult`, which reads as the
  outcome of running, and not `ParsedCli`, which breaks the `Cli` prefix.
- `ParseCommandLine()`, `RunCommandLine()`, `FormatCliHelp()`, and
  `ValidateCliCommand()`.
- `CliText`, `CliWriter`, `WriteCliText()`, `kConstraintCliWriter`, and
  `CliContract` (see CL6's names), and `kConstraintCliHelpWidth` (see CL7's).
- `GetMainArgs()`: the arguments `main()` is given. Not `GetCommandLine...`,
  which is a Windows macro family (`GetCommandLineW()`), and not `CliArgs`,
  which reads as a list of `CliArg` definitions.

### Files

- `cli.h`: the definitions (`CliCommand`, `CliFlag`, `CliArg`, `CliChoice`,
  `CliAnyFlag`, `CliAnyArg`) and `ValidateCliCommand()`.
- `cli_internal.h`: what the library's source files share, private to
  `gb_cli` and its tests: the fields every flag or argument has, parsing a
  value for any of them, and `GetCliPathName()`, for errors and help.
- `cli_validators.h`: the common validators.
- `cli_command_line.h`: `CliCommandLine`, `ParseCommandLine()`,
  `kConstraintCliWriter`, `kConstraintCliHelpWidth` (with
  `kKeyCliHelpWidth` and `kDefaultCliHelpWidth`), and `CliContract`.
- `cli_help.h`: `kCliHelpCommand` and `FormatCliHelp()`.
- `cli_writer.h`: `CliText`, `CliWriter`, and `WriteCliText()`.
- `fake_cli_writer.h`: `FakeCliWriter`, which keeps what is written to the
  `CliWriter` it makes, for `gb_cli`'s own tests.
- `run_command_line.h`: `RunCommandLine()` and `GetMainArgs()`. Nothing in the
  library is platform-specific.

### Value types

```
// A flag of any value type, as a command lists it: `{&kOutput, &kVerbose}`.
using CliAnyFlag =
    std::variant<const CliFlag<bool>*, const CliFlag<std::string_view>*,
                 const CliFlag<int64_t>*, const CliFlag<double>*>;

// A positional argument of any value type, as a command lists it.
using CliAnyArg =
    std::variant<const CliArg<std::string_view>*, const CliArg<int64_t>*,
                 const CliArg<double>*>;
```

- The value types are a fixed set, so a command's lists are variants of
  pointers to each type's definition, and `&kOutput` converts to one. The
  library's sources `std::visit` them, so parsing and validating definitions
  are in the `.cc` files, and `cli.h` holds only what a program writes, apart
  from `internal::CliNotConstexpr`.
- `CliCommandLine` maps each definition's address to its value, an
  `internal::CliValue` (`std::variant<bool, std::string_view, int64_t,
  double>`), in an `absl::flat_hash_map`. `Get<T>()` looks it up, so a read
  costs one hash lookup. It is defined in `cli_command_line.cc`, and explicitly
  instantiated for each value type.
- CL1 and CL2 allowed any enumeration as a value type too. That left the type
  open, so each type's parsing had to be instantiated from the header and
  hidden behind type-erased refs in a command's lists. CL3 replaced
  enumerations with string choices, which the program maps to its own enum.
- Parsing is a single pass over the arguments, walking down the command tree.
  It costs nothing that matters: it runs once per program.

**Brittleness:**
- A definition must be static data (see Definitions); one built as a local and
  copied out leaves its lists dangling. Definitions are documented as
  constants with static storage, and the `Behavior` example is the pattern.
  `constexpr`, which would silently break on MSVC, fails to compile.
- A flag must be in its command's `flags` to be read. `Get()` CHECKs if it
  isn't, which any test of the command finds.
- The value types are listed five times: the `static_assert`s in `CliFlag`
  and `CliArg`, `CliAnyFlag`, `CliAnyArg`, and `CliValue`. A new type is
  added to each, given a `ParseText()` overload in `cli_internal.cc` (whose
  absence fails to compile), and an explicit instantiation of `Get()` in
  `cli_command_line.cc` (whose absence fails to link).
- A `CliFlag<bool>` has fields it can't use (`value_name`, `choices`,
  `default_value`, `validators`), and a number has `choices`.
  `ValidateCliCommand()` rejects them, so any run of the program finds one
  that is set.

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
- ~~Help counts code points with utfcpp (`utf8::distance()`), which `gb_base`
  already uses privately.~~ CL4 counts the bytes that aren't UTF-8
  continuation bytes instead, which needs no dependency: utfcpp would have
  been a new include directory for `gb_cli`, and `gb_base`'s `unicode.h` has
  no count of code points.

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

### CL3 [x] gb_cli: a fixed set of value types

Depends on: CL2.

- The value types are fixed: `bool`, `std::string_view`, `int64_t`, and
  `double`. A command lists its flags and arguments as `CliAnyFlag` and
  `CliAnyArg`, variants of pointers to each type's definition, so parsing and
  validating definitions move out of `cli.h` into the library's sources, and
  the type-erased refs go.
- Enumerations are replaced by `choices` on a string value, each a
  `CliChoice` of a name and its help.

**Verify**
- Standard checks.
- Unit tests: choices given and defaulted, a value that isn't a choice, and
  each choice definition error (choices on a type other than a string, and an
  empty, duplicate, or "-" prefixed choice). The other tests carry over.
- By hand: a flag of an enumeration, or of `int`, fails to compile, and so
  does a `CliArg<bool>`.

### CL4 [x] gb_cli: help

Depends on: CL3.

- `--help` and `-h` on every command, and `CliValues::Run()`, which runs the
  command or writes its help.
- `cli_help.h`: `kCliHelpCommand` and `FormatCliHelp()`.

**Verify**
- Standard checks.
- Unit tests: the full help text for a program with arguments, one with
  subcommands and inherited flags, a flag with choices, defaults, long flag
  names that push help onto the next line, and wrapping. `--help` in each
  position, with invalid arguments after it, and `help <command>`. `Run()`
  runs the command and returns its result, writes help instead when asked or
  when there is no run function, and `Get()` CHECKs on help's values.

### CL5 [x] gb_cli: rename CliValues to CliCommandLine

Depends on: CL4.

- `CliValues` becomes `CliCommandLine`, the parsed command line, which holds
  more than values: the command path, whether help was asked for, and
  `Run()`. `cli_values.h` becomes `cli_command_line.h`. No change in behavior.

**Verify**
- Standard checks. The tests carry over, renamed.

### CL6 [x] gb_cli: context and running a command line

Depends on: CL5.

- The context (see Context): `cli_writer.h` (`CliText`, `CliWriter`,
  `WriteCliText()`), `kConstraintCliWriter` (defaulting to `WriteCliText()`)
  and `CliContract`, `ParseCommandLine()` taking a
  `CliContract`, `CliCommandLine::Run()` writing help to the context's writer,
  and `CliCommand::constraints`, with run functions getting the validated
  context. `gb_cli` depends on `gb_base` again (tier 0).
- `run_command_line.h`: `RunCommandLine()` and `GetMainArgs()` (see Running).
- Parse errors end with how to get help for the command so far.
- The backlog's *Path validators for command lines* says validators would get
  the context, which is where the `FileSystem` would be.

**Verify**
- Standard checks.
- Unit tests, with a fake `CliWriter` in the context that collects text:
  - `Run()` writes help to the context's writer, and a run function gets the
    command line and the context.
  - A run function reads a value its command's constraints allow, and one
    allowed by a command above it.
  - A command's required value that is missing: `Run()` returns false
    without running it (with the `ValidatedContext` error callback set to
    collect the error, rather than `LOG(FATAL)`).
  - `RunCommandLine()`'s exit codes (0, 1, 2), and the text written for help
    and errors.
  - Each parse error ends with the right `Run "... --help"` hint.
  - `GetMainArgs()` skips the program's name, and returns nothing for an
    `argc` of 1.
  - Parsing adds the default writer to a context that has none.
- `WriteCliText()` itself (stdout and stderr) is the only part left untested.

**Names**
- `CliWriter`: what text is written to. `CliOutput` is vaguer, and
  `CliConsole` suggests a terminal, when the text may go to a window or a log.
  Game Bits' other writers (`ChunkWriter`, `ResourceFileWriter`) write files,
  but have their own prefixes, so they don't collide.
- `CliText` and `WriteCliText()`: what a piece of text is, and the default
  writer, which writes to C stdio's stdout and stderr, whatever they are
  attached to.
- `kConstraintCliWriter` and `CliContract`, following `Game`'s
  `kConstraintClock` and `GameContract`.

**Brittleness**
- A context passed as a `Context*` must outlive the `CliCommandLine`, as the
  arguments already must. An owned context (`std::make_unique<Context>()`)
  doesn't.
- A command's required values are only validated when it runs, as which
  command runs is only known then. A program's tests of each command find a
  missing one.
- A program's writer must be set as a `CliWriter`:
  `context.SetValue<CliWriter>(WriteToLog)`. `CliWriter` is a `std::function`,
  so a lambda or function given to `SetValue` without the type would be stored
  as its own type, which gb_cli doesn't look up. `SetValue` needs its type
  written, so this is hard to get wrong.
- The default writer is added to a context passed by pointer, and stays
  there, as `kInOptional` defaults do throughout Game Bits.
- `RunCommandLine()` holds the context itself, and parses with a
  `CliContract` that refers to it, so the writer is still there for an error
  when parsing fails. A null constraint in a command is a definition error,
  rather than a crash when the command runs.

### CL7 [x] gb_cli: help width from the context

Depends on: CL6.

- `kConstraintCliHelpWidth` (see Context), an optional `int` named
  `"cli_help_width"` that defaults to `kDefaultCliHelpWidth` (80), added to
  `CliContract`.
- `FormatCliHelp()` takes the width, and `CliCommandLine::Run()` passes the
  context's.

**Verify**
- Standard checks.
- Unit tests:
  - Help wraps to a width other than 80.
  - `Run()` wraps help to a width set in the context.
  - Parsing adds the default width to a context that has none.

**Names**
- `kConstraintCliHelpWidth`, `kKeyCliHelpWidth`, and `kDefaultCliHelpWidth`,
  following `Game`'s `kConstraintMaxFps`, `kKeyMaxFps`, and `kDefaultMaxFps`.
  Not `kConstraintCliWidth`, which reads as wrapping errors too, and "width"
  rather than "columns", as the formatter already says.

**Brittleness**
- A width set under another name is a different value, so help is wrapped to
  80 without an error. `kKeyCliHelpWidth` makes this unlikely. A width of
  another type (`SetValue<int64_t>`) under the name is a validation error.

### CL8 [x] gb_cli: validators that use the context

Depends on: CL7.

- `CliValidator<T>` becomes a definition of a function that takes the context
  and the value, and the constraints of what it reads, which flags and
  arguments list by address (see Validators and Context). The common
  validators become constants: `kCliInRange<T, kMin, kMax>`,
  `kCliAtLeast<T, kMin>`, and `kCliAtMost<T, kMax>` (variable templates), and
  `kCliNotEmpty`. Their functions move to `internal`.
- The parser validates the context against the validators' constraints of
  each command it reaches, and passes the validated context to them.
  `ParseCommandLine()` returns a `FailedPrecondition` error for a context that
  is missing a value, or fails `gb_cli`'s own constraints, and
  `RunCommandLine()` returns 1 for it.
- `ValidateCliCommand()` rejects a null validator, a validator with no
  function, and a null constraint or one that isn't an input.

**Verify**
- Standard checks.
- Unit tests:
  - A validator reads a value its constraints allow from the context, for a
    flag of the command and one of a command above it, and for an argument.
  - A missing value fails parsing when the command is reached, even if the
    flag isn't given, with the `ValidatedContext` error callback collecting
    the error.
  - A context that fails `gb_cli`'s constraints fails parsing, and
    `RunCommandLine()` returns 1 for it and for a missing value.
  - The definition errors for validators.

**Brittleness**
- A validator with constraints is only checked against the context when its
  command is reached, so help for that command (`prog build --help`) fails too
  when the program's context is missing the value. That is the program's
  mistake, and is found on any run naming the command.

### CL9 [ ] gb_cli: path validators

Depends on: CL8.

- `cli_path.h`: `kConstraintCliFileSystem`, `kConstraintCliFileProtocol`
  (`"cli_file_protocol"`, defaulting to `"file"`), `GetCliFilePath()`, and
  the validators `kCliPathExists`, `kCliFileExists`, and `kCliFolderExists`.
  Paths are converted with `LocalFileProtocol::ToOsRootPath()`, so the
  protocol must take its paths, as one from `CreateOsRoot()` does.
- `gb_cli` depends on `gb_file`.
- *Path validators for command lines* comes out of the backlog.

**Verify**
- Standard checks.
- Unit tests, with a `MemoryFileProtocol` registered as `"file"`, and files
  at the `ToOsRootPath()` paths: each validator accepts and rejects paths,
  relative and absolute, `GetCliFilePath()` uses the context's protocol, and a
  missing `FileSystem` fails parsing.
