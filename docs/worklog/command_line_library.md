# Command line library

`gb_cli` defines a program's command line in one place, as constant data,
rather than by flags declared throughout the code. A program defines its
commands, their flags and positional arguments, and the help for each, then
hands `main`'s arguments to `RunCommandLine()`. That parses them, validates the
values, and runs the function bound to the command they name, or writes the
help text, or writes what is wrong with the command line.

A command line is parsed and run with a `gb::Context`, which holds where text
goes, the width help wraps to, and whatever the program's commands and
validators read (a `gb::FileSystem` for paths, the program's own settings).
Each piece of code that reads the context declares what it reads, and gets a
`ValidatedContext` that allows only that.

OZ-3 is the first user (*Command lines on Game Bits*): `oz3ism` and `oz3asm`
keep the short flags the wiki documents (`oz3asm -i <instruction set> -m -l
<source file> [<program file>]`).

## Using it

```
const gb::CliFlag<std::string_view> kInstructionSet = {
    .name = "instruction-set",
    .short_name = 'i',
    .value_name = "instruction set",
    .help = "Assembles for the instruction set in this source file (.izm), "
            "rather than the default one.",
    .validators = {&gb::kCliFileExists},
};
const gb::CliFlag<bool> kMap = {
    .name = "map",
    .short_name = 'm',
    .help = "Writes the program's map beside the program file (.oz3map).",
};
const gb::CliArg<std::string_view> kSourceFile = {
    .name = "source file",
    .help = "The program to assemble.",
    .validators = {&gb::kCliFileExists},
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
    .constraints = {&gb::kConstraintCliFileSystem,
                    &gb::kConstraintCliFileProtocol},
    .run = Run,
};

bool Run(gb::ValidatedContext& context,
         const gb::CliCommandLine& command_line) {
  gb::FileSystem* file_system = context.GetPtr<gb::FileSystem>();
  const std::string source_path =
      gb::GetCliFilePath(context, *command_line.Get(kSourceFile));
  const bool write_map = command_line.Get(kMap);
  ...
}

int main(int argc, char* argv[]) {
  gb::FileSystem file_system;
  file_system.Register(gb::LocalFileProtocol::CreateOsRoot());
  file_system.SetDefaultProtocol("file");
  gb::Context context;
  context.SetPtr<gb::FileSystem>(&file_system);
  return gb::RunCommandLine(&context, &kProgram, gb::GetMainArgs(argc, argv));
}
```

A program with nothing in its context calls `RunCommandLine(&kProgram, args)`.

## Behavior

### Definitions

- **Constant data:** every definition (`CliCommand`, `CliFlag<T>`,
  `CliArg<T>`, `CliChoice`, `CliValidator<T>`) is a `const` aggregate, written
  with designated initializers, with static storage. Its lists are initializer
  lists, so a definition holds pointers into constant data, and definitions
  refer to each other only by address. A definition must not be a temporary or
  a local copied elsewhere.
- **Not constexpr:** each definition with lists holds an
  `internal::CliNotConstexpr` member, so a `constexpr` definition fails to
  compile (see Deliberate choices). Definitions are initialized at startup,
  and must not be read before `main()`.
- **Commands:** a `CliCommand` has a name, help, flags, positional arguments,
  subcommands, the context constraints its run function reads and writes, and
  an optional run function, `bool (*)(ValidatedContext&, const
  CliCommandLine&)`. The program is the root command, and its name is used in
  help rather than `argv[0]`. A command has arguments or subcommands, not both.
  A command with no run function writes its help when it is run.
- **Flags:** a `CliFlag<T>` has a long name (`--name`), an optional short name
  (`-n`), a value name shown in help (defaulting to the name), help, choices,
  a default, and validators. A `CliFlag<bool>` takes no value: it is true if
  given and false otherwise.
- **Positional arguments:** a `CliArg<T>` has a name, shown in help as
  `<name>`, help, choices, and validators. Optional arguments
  (`.optional = true`) follow the required ones, and may have a default.
- **Value types:** `std::string_view`, `int64_t`, and `double`, and `bool` for
  flags only. Any other type fails to compile. A string value points into the
  arguments, which must outlive it.
- **Choices:** a string flag or argument that accepts only some values lists
  them, each a `CliChoice` of a name and optional help. The program maps the
  chosen string to its own enum if it has one.
- **Numbers:** the text must be exactly the number: a base 10 integer with an
  optional sign, or a finite floating point number, in range, with no
  surrounding space.
- **Definition errors:** `ValidateCliCommand()` finds a definition that can't
  be parsed unambiguously, and `ParseCommandLine()` CHECKs it, so any run of
  the program finds one: empty or duplicate names (a flag's names are unique
  across its command and those above it), a name starting with `-`, a short
  name that is `-`, `.`, a digit, or not visible ASCII, a flag named `--help`
  or `-h`, an optional argument before a required one, arguments and
  subcommands on one command, a bool flag with a value name, default, or
  validators, choices on a type other than a string, a choice that is empty,
  a duplicate, or starts with `-`, a null constraint on a command, and a null
  validator, one with no function, or one with a constraint that is null or
  isn't an input.

### Validators

- A flag or argument lists its validators by address, each a
  `CliValidator<T>`: a function, `absl::Status (*)(const ValidatedContext&,
  const T&)`, returning an error saying what is wrong with the value, and the
  constraints of what it reads from the context, which must be inputs.
- A validator's `InvalidArgument` error is the user's mistake. Any other error
  is the program's, such as a context value naming something the context
  doesn't have, and `ParseCommandLine()` returns it as a `FailedPrecondition`
  error, without saying how to get help.
- Common validators, in `cli_validators.h`: `kCliInRange<T, kMin, kMax>`,
  `kCliAtLeast<T, kMin>`, and `kCliAtMost<T, kMax>` (variable templates of the
  value type and bounds: `{&gb::kCliInRange<int64_t, 1, 64>}`), and
  `kCliNotEmpty`. Path validators, in `cli_path.h`, are under Paths.
- A default value isn't validated.
- When the parser reaches a command, it validates the context against the
  constraints of the validators of its flags and arguments and those of the
  commands above it, whether or not they are given. A missing value is
  reported as `ValidatedContext` reports any error (`LOG(ERROR)`, or a
  program's callback), and `ParseCommandLine()` returns a `FailedPrecondition`
  error, as it is the program's mistake, not the user's.
- Each validator is then called with a `ValidatedContext` of its own, with
  only its own constraints, so it can only read what it declares.

### Parsing

```
absl::StatusOr<CliCommandLine> ParseCommandLine(
    CliContract contract, const CliCommand* program,
    absl::Span<const std::string_view> args);
absl::StatusOr<CliCommandLine> ParseCommandLine(
    const CliCommand* program, absl::Span<const std::string_view> args);
```

- `args` are the arguments after the program's name.
- Flags: `--name value`, `--name=value`, `-n value`, and for bool flags
  `--name` or `-n`. Short bool flags don't combine (`-ml` is an unknown flag).
  The value after a flag is taken as it is, even if it starts with `-`.
- A bare word is the next subcommand's name if the command has subcommands,
  and otherwise its next positional argument. After `--`, every word is a
  positional argument. `-` alone is a positional argument, and so is a word
  starting with `-` then a digit or `.`, so negative numbers can be given.
- A command's flags are accepted anywhere after its name, including after its
  subcommands' names, and can be read by the command that runs.
- Errors are an `InvalidArgument` status whose message is ready to show: an
  unknown flag or command, a flag given twice, a missing value, a value that
  isn't of the type or one of the choices, a value a validator rejects, a
  missing required argument, or an extra argument. Each names the command
  path, and says how to get help: `oz3asm: unknown flag "--foo". Run "oz3asm
  --help" for help.`
- A context that fails `gb_cli`'s own constraints (a null `Context*`, or a
  width that isn't an `int`), or is missing a value a validator reads, is a
  `FailedPrecondition` error, as is a validator's error other than
  `InvalidArgument`: `oz3asm: <source file>: paths are opened with the "src"
  protocol, which the file system doesn't have`.

### Values and running a command

```
class CliCommandLine {
 public:
  const CliCommand* GetProgram() const;
  const CliCommand* GetCommand() const;  // The command that runs.
  absl::Span<const CliCommand* const> GetCommandPath() const;  // Root first.

  // Runs the command, or writes its help to the context's writer.
  bool Run();

  bool Get(const CliFlag<bool>& flag) const;
  template <typename T> std::optional<T> Get(const CliFlag<T>& flag) const;
  template <typename T> std::optional<T> Get(const CliArg<T>& arg) const;
};
```

- The `CliCommandLine` keeps the context it was parsed with, so it is
  move-only. A context passed as a `Context*` must outlive it.
- `Get()` returns the given value, else the default, else `std::nullopt`. A
  required argument is always set, and a bool flag reads as a plain `bool`.
  `Get()` CHECKs on a flag or argument outside the command path, and on the
  values of a request for help.
- `Run()` writes the command's help if it was asked for, or the command has no
  run function, and returns true. Otherwise it validates the context against
  `CliContract` and the constraints of every command in the path, and calls
  the run function with that `ValidatedContext`, returning its result. A
  missing value, or a `kOut` value the command didn't add, is reported as a
  context error, and `Run()` returns false.

### Help

- Every command accepts `--help` and `-h`, which stop parsing and ask for the
  help of the command so far. Nothing after them is checked.
- `kCliHelpCommand` is a `help` command a program can add to any command's
  subcommands: `prog help build` is `prog build --help`.
- `FormatCliHelp(path, width)` returns the help for the command at the end of
  the path: a usage line, the help paragraphs, then the arguments, the
  subcommands (with the first paragraph of their help), the flags, and those
  of the commands above it, nearest first. Choices are listed with their help,
  and defaults as `(default: <value>)`.
- Help is in a column shared by all the lists, and an entry wider than 26
  columns has its help start on the next line. Lines wrap at words, by code
  point, up to the width; the usage line doesn't wrap.

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

### The context

`CliContract` is what `gb_cli` reads from the context, and each optional value
is added to it if it has none:
- `kConstraintCliWriter`: a `CliWriter`, `std::function<void(CliText type,
  std::string_view text)>`, where help (`CliText::kHelp`) and errors
  (`CliText::kError`) go. It defaults to `WriteCliText()`, which writes help
  to stdout and errors to stderr. A command can write its own errors with it.
- `kConstraintCliHelpWidth`: an `int` named `kKeyCliHelpWidth`
  (`"cli_help_width"`), defaulting to `kDefaultCliHelpWidth` (80).

### Paths

`cli_path.h` opens paths given on the command line in a `FileSystem`:
- `kConstraintCliFileSystem`: the `FileSystem`, required.
- `kConstraintCliFileProtocol`: an optional `std::string` named
  `kKeyCliFileProtocol` (`"cli_file_protocol"`). Without one, or with an empty
  one, paths use the file system's default protocol.
- `GetCliFilePath(context, path)` converts a path written the operating
  system's way, absolute or relative to the current directory, with
  `LocalFileProtocol::ToOsRootPath()`, and prefixes the context's protocol if
  it names one (`"file:/c/src/main.txt"`). It returns an empty string for a
  path that can't be converted (an empty path, or a network path). The
  protocol must take the paths of one from `CreateOsRoot()`.
- `kCliPathExists`, `kCliFileExists`, and `kCliFolderExists` validate that a
  path exists, is a file, or is a folder: `"x" doesn't exist`, `"x" isn't a
  file`, `"x" isn't a folder`. Each reads both constraints, so a context
  without a `FileSystem` fails parsing. A path that can't be converted doesn't
  exist, as far as they say. If the file system doesn't have the context's
  protocol, or a default protocol when the context names none, they fail as
  the program's mistake.
- A run function that calls `GetCliFilePath()` lists both constraints in its
  command's constraints, as the example does.

### Running a program

```
int RunCommandLine(CliContract contract, const CliCommand* program,
                   absl::Span<const std::string_view> args);
int RunCommandLine(const CliCommand* program,
                   absl::Span<const std::string_view> args);
std::vector<std::string_view> GetMainArgs(int argc, char* argv[]);
```

- `RunCommandLine()` parses the command line, writes the error to the
  context's writer if it isn't valid, and otherwise calls
  `CliCommandLine::Run()`. It returns `main`'s exit code: 0 if the command
  succeeded or help was written, 1 if the command returned false or the
  program made a mistake (`FailedPrecondition`), and 2 if the command line
  isn't valid. A context that fails `gb_cli`'s own constraints may have no
  writer, so nothing is written for it beyond what `ValidatedContext` reports.
- `GetMainArgs()` returns views of `main()`'s arguments after the program's
  name, which stay valid for the rest of the program.
- Arguments and definitions are UTF-8, and pass through as bytes. On Windows,
  `argv` is UTF-8 because the Game Bits CMake commands embed a manifest in
  every executable that makes the process's code page UTF-8.

## Structure

`gb_cli` is a tier 1 library, depending on Abseil, `gb_base` (for
`gb::Context`), and `gb_file` (for paths). Nothing in it is
platform-specific.

| File | What |
|---|---|
| `cli.h` | The definitions (`CliCommand`, `CliFlag`, `CliArg`, `CliChoice`, `CliValidator`, `CliAnyFlag`, `CliAnyArg`) and `ValidateCliCommand()` |
| `cli_validators.h` | The common validators |
| `cli_path.h` | `kConstraintCliFileSystem`, `kConstraintCliFileProtocol`, `GetCliFilePath()`, and the path validators |
| `cli_command_line.h` | `CliCommandLine`, `ParseCommandLine()`, `kConstraintCliWriter`, `kConstraintCliHelpWidth`, and `CliContract` |
| `cli_help.h` | `kCliHelpCommand` and `FormatCliHelp()` |
| `cli_writer.h` | `CliText`, `CliWriter`, and `WriteCliText()` |
| `run_command_line.h` | `RunCommandLine()` and `GetMainArgs()` |
| `cli_internal.h` | Private to `gb_cli` and its tests: the fields every flag or argument has, parsing a value, gathering validators' constraints, and `GetCliPathName()` |
| `fake_cli_writer.h` | `FakeCliWriter`, which keeps what is written, for `gb_cli`'s tests |

- **Value types:** the types are a fixed set, so a command's lists are
  variants of pointers to each type's definition (`CliAnyFlag`, `CliAnyArg`),
  and `&kOutput` converts to one. The sources `std::visit` them, so parsing
  and validating definitions are in the `.cc` files, and `cli.h` holds only
  what a program writes. `CliCommandLine` maps each definition's address to
  its value, an `internal::CliValue` variant, in an `absl::flat_hash_map`, so a
  read is one hash lookup.
- **Parsing:** `CliParser` makes a single pass over the arguments, walking
  down the command tree. On entry, and on each subcommand, it validates the
  context against the constraints of every validator in the path, in a
  temporary `ValidatedContext` used only as that check. Each value is parsed
  by `internal::ParseCliValue()`, which builds each validator's own context
  from the parser's. It costs nothing that matters: it runs once per program.
- **Help:** `FormatCliHelp()` builds the text with a `HelpWriter` that wraps
  to the width it is given. It counts code points as the bytes that aren't
  UTF-8 continuation bytes, which needs no dependency.

**Brittleness:**
- A definition must be static data. One built as a local and copied out leaves
  its lists dangling. `constexpr`, which would silently break on MSVC, fails
  to compile.
- A flag must be in its command's `flags` to be read. `Get()` CHECKs if it
  isn't, which any test of the command finds.
- The value types are listed five times: the `static_assert`s in `CliFlag` and
  `CliArg`, `CliAnyFlag`, `CliAnyArg`, and `CliValue`. A new type also needs a
  `ParseText()` overload in `cli_internal.cc` (or it fails to compile), and an
  explicit instantiation of `Get()` in `cli_command_line.cc` (or it fails to
  link).
- A program's writer must be set as a `CliWriter`
  (`context.SetValue<CliWriter>(WriteToLog)`), or it is stored as its own type,
  which `gb_cli` doesn't look up.
- A width or protocol set under another name is a different value, so it is
  silently not used. The `kKey` constants make this unlikely. A value of the
  wrong type under the name is a validation error.
- A command's required context values are only validated when it runs, as
  which command runs is only known then. A program's tests of each command
  find a missing one.
- A run function that calls `GetCliFilePath()` must list
  `kConstraintCliFileProtocol`. If it doesn't, reading it is reported as a
  context error on the first run, and the path is for the default protocol.
- The protocol paths are opened with must take the paths of one from
  `CreateOsRoot()`. One rooted elsewhere is given paths it reads as under its
  root, without an error, which the program's first run finds.

## Deliberate choices

- **`const`, not `constexpr`:** MSVC (19.44) compiles a `constexpr` aggregate
  with an initializer list member, but leaves the list's elements zero at run
  time. `const` definitions are correct, and the `CliNotConstexpr` member
  makes `constexpr` a compile error. Those definitions are initialized
  dynamically, which is safe because they refer to each other only by address.
- **Validators by address:** a flag or argument lists its validators by
  address rather than holding copies. A copy of a validator defined in another
  file could be made before that file's definitions are initialized (the
  static initialization order problem), leaving an empty validator. The cost
  is that every validator is a named definition, and a number validator names
  its value type, as a variable template can't take it from the list it is
  in.
- **Every validator takes the context,** so there is one kind of validator. A
  validator with nothing to read leaves its constraints empty.
- **A context for each piece of code:** a validator gets a context with only
  its own constraints, and a run function one with only `CliContract` and its
  commands' constraints, rather than one with everything any of them
  declared. Each declares everything it reads, so none works only because
  another declared a value, and then fails in some other configuration. The
  parser still checks all of a command's validators' constraints when it
  reaches the command, so a missing value is found on any run naming the
  command, not only when the flag that reads it is given.
- **Help needs the context too:** as validators' constraints are checked when
  their command is reached, `prog build --help` fails when the program's
  context is missing a value one of `build`'s validators reads. That is the
  program's mistake, and found on any run naming the command.
- **Choices instead of enumerations:** enumeration value types left the set of
  types open, so each type's parsing had to be instantiated from the header,
  behind type-erased references. String choices keep the types fixed, and the
  program maps a choice to its enum.
- **Number types:** only the largest of each kind (`int64_t`, `double`), as a
  command line has no use for smaller ones.
- **`CliWriter` is a `std::function`:** a context default is a copyable value
  in a `std::any`, so it can't be an interface. It also leaves no derived type
  for `SetPtr(&writer)` to store it under by mistake.
- **Writing only to the writer:** `RunCommandLine()` writes nothing directly,
  so a program can send its text elsewhere (a log, a window), and tests
  collect it.
- **The default protocol for paths:** paths use the file system's default
  protocol unless the context names one, so a program that registers its
  `CreateOsRoot()` protocol as the default sets nothing else.
- **Converting paths in `gb_cli`:** a conversion hook on each protocol, or
  asking the file system how to read a path, were set aside, as they work
  around a Windows drive reading as a protocol name rather than fix it (see
  Known limitations).
- **Short bool flags don't combine** (`-ml`), which keeps every short flag
  unambiguous.

## Known limitations

Each is a backlog item:

- **Command line paths without conversion:** every path is converted with
  `ToOsRootPath()`, which ties the path validators to a protocol that takes
  the paths of one from `CreateOsRoot()`.
