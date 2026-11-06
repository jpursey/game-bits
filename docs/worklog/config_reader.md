# Config reader with error locations

A program reads a `Config` into its own settings with a `ConfigReader`, which
checks each value's type and range, rejects unknown keys, and returns the
first problem it finds with where it is:
`hello.ozzy(12:5): devices[1].columns: must be 1 to 132`. Today a program
reading a config file by hand writes these checks itself (OZ-3's
`ReadMachineSettings` has a page of helpers for them), and its errors can only
name the path, not the line in the file.

The reader keeps each value's path as it goes down from the root. Paths aren't
stored in `Config`: a `Config` is a value that gets copied and edited, so a
stored path would go stale, and the reader knows the path anyway. Locations
are stored: `ReadConfigFromText` records each value's line and column, and a
copy keeps them, as it is still the value read from there.

## Behavior

### Locations

```
// Where a config value was read from, if it was read from text.
struct ConfigLocation {
  bool operator==(const ConfigLocation&) const = default;

  // Returns true if this is a location, rather than no location.
  bool IsSet() const { return line > 0; }

  int line = 0;    // 1-based line, or 0 if the value has no location.
  int column = 0;  // 1-based column, in bytes from the start of the line.
};

class Config {
  ...
  // Returns where this value was read from. A value made or replaced in code
  // has no location. Copies keep their location.
  ConfigLocation GetLocation() const;

  // Only the config module's text reader sets a location.
  void SetLocation(ConfigInternal, ConfigLocation location);
};
```

- A setter that replaces the whole value (`SetInt(5)`, `SetMap(...)`,
  `Clear()`, and `Set(key, ...)`, `Set(0, ...)`, `Append()` or
  `ResizeArray()` when they turn the value into a map or array) clears its
  location. Setting a key of an existing map, or an element of an existing
  array, keeps the map's or array's location; the new value brings its own.
- `ReadConfigFromText` sets every value's location to its first token: the
  value itself for a scalar, `{` or `[` for a map or array. The root of a
  rootless file is at its first key, or at the end of the text if it has none.
- `ReadConfigFromText` gains an optional `filename`, after `flags`, which
  syntax errors then name: `hello.ozzy(3:1): Parse error at "b": Invalid key`,
  or `hello.ozzy: ...` for the few errors with no location (such as a line too
  long to read). Existing callers are unchanged.
- `WriteConfigToText` ignores locations.

### Reading

```
// An integer type ReadInt() can read into: any integral type but bool (and the
// character types), which ReadBool() and ReadString() read instead.
template <typename T>
concept ConfigInt = std::integral<T> && !std::same_as<T, bool> &&
                    !std::same_as<T, char> && !std::same_as<T, char8_t> &&
                    !std::same_as<T, char16_t> && !std::same_as<T, char32_t> &&
                    !std::same_as<T, wchar_t>;

// The name an enum value has in a config, for ConfigReader::ReadEnum():
//   constexpr gb::ConfigEnumName<DeviceType> kDeviceTypes[] = {
//       {"screen", DeviceType::kScreen}, {"keyboard", DeviceType::kKeyboard}};
template <typename T>
  requires std::is_enum_v<T>
struct ConfigEnumName {
  std::string_view name;
  T value;
};

// Reads a config into a program's own settings. Each reader is a view of one
// value, with its path from the root. Every reader made from the same root
// shares that read's status, which keeps the first error any of them finds.
class ConfigReader final {
 public:
  // Reads `config`, which must outlive this and every reader made from it.
  // Errors begin with `source` (such as the file the config was read from), if
  // it isn't empty.
  explicit ConfigReader(const Config* config, std::string_view source = {});

  // Copyable: a copy is the same view of the same read.

  // Returns OK, or the first error any reader of this read has found.
  absl::Status GetStatus() const;

  // The value, or null if it is missing. Its path ("devices[1].columns", or
  // empty for the root).
  const Config* GetConfig() const;
  std::string_view GetPath() const;
  bool IsMissing() const;

  // Returns the value at `key`, or a missing value if there is no such key.
  // Rejects this value if it is present and isn't a map.
  ConfigReader Get(std::string_view key) const;

  // Returns the value at `index`, or a missing value if it is out of range.
  // Rejects this value if it is present and isn't an array.
  ConfigReader Get(int index) const;
  int GetArraySize() const;

  // Returns true if this is a map with no keys but `keys`. Rejects it if it
  // isn't a map, or names the first unknown key in sorted order (maps are
  // unordered). Returns false, with no error, if it is missing.
  bool ValidateMap(absl::Span<const std::string_view> keys) const;

  // Returns true if this is an array of `min_size` to `max_size` values.
  // Rejects it if not. Returns false, with no error, if it is missing.
  bool ValidateArray(int min_size = 0,
                     int max_size = std::numeric_limits<int>::max()) const;

  // Reads this value into `value`, rejecting it if it is the wrong type or out
  // of range. A missing value leaves `value` as it was, so it keeps the
  // caller's default.
  void ReadBool(bool* value) const;
  template <ConfigInt T> void ReadInt(T* value) const;  // Must fit in T.
  template <ConfigInt T>
  void ReadInt(int64_t min, int64_t max, T* value) const;
  template <std::floating_point T>
  void ReadFloat(T* value) const;  // An int in the config is fine too.
  void ReadString(std::string* value) const;

  // Reads a string naming one of `names` into `value`. Rejects anything else,
  // listing the names: `must be "screen", "keyboard", or "teletype"`.
  template <typename T>
    requires std::is_enum_v<T>
  void ReadEnum(absl::Span<const ConfigEnumName<T>> names, T* value) const;

  // Records an error at this value, if the read has none yet:
  // "<source>(<line>:<column>): <path>: <message>".
  void Reject(std::string_view message) const;
};
```

Used like this (much of OZ-3's `ReadMachineSettings`):

```
gb::ConfigReader reader(&config, filename);
if (reader.ValidateMap(kMachineKeys)) {
  reader.Get("clock_hz").ReadInt(1, MachineSettings::kMaxClockHz,
                                 &settings.clock_hz);
  reader.Get("cores").ReadInt(1, kMaxCores, &settings.cores);
  const gb::ConfigReader banks = reader.Get("banks");
  if (banks.ValidateArray(1, kMaxMemoryBanks)) {
    settings.banks.assign(banks.GetArraySize(), 0);
    for (int i = 0; i < banks.GetArraySize(); ++i) {
      const gb::ConfigReader bank = banks.Get(i);
      if (bank.ValidateMap(kBankKeys)) {
        bank.Get("ram").ReadInt(0, 0xFFFF, &settings.banks[i]);
      }
    }
  }
  const gb::ConfigReader device = reader.Get("devices").Get(0);
  const gb::ConfigReader type = device.Get("type");
  if (type.IsMissing()) {
    type.Reject("missing type");
  }
  DeviceType device_type = DeviceType::kNone;
  type.ReadEnum<DeviceType>(kDeviceTypes, &device_type);
  switch (device_type) { ... }
}
if (!reader.GetStatus().ok()) {
  return reader.GetStatus();
}
```

- Errors are `InvalidArgument`. The text is formatted as `ParseError` formats
  a syntax error, so both kinds read alike: `hello.ozzy(12:5): path: message`.
  The parts that aren't known are left out: no source gives `(12:5): ...`
  (as `ParseError` does), no location gives `hello.ozzy: ...`, and the root
  has no path.
- A missing value's errors give the location of the value it is missing from,
  so `device.Get("type").Reject("missing type")` points at the device's `{`.
- Messages: `must be a map`, `must be an array`, `must be a bool`, `must be an
  int`, `must be a float`, `must be a string`, `must be 1 to 132`, `must have 1
  to 8 values` (or `must have 2 values` and `must have at least 1 value` for
  an exact or open size), and `unknown key` (at the key's own path). A float
  too big for a `float` gives its range: `must be -3.40282e+38 to
  3.40282e+38`.
- The destination types are constrained, so a mismatch is a compile error
  rather than a silent conversion: `ReadInt()` takes a pointer to an integer
  type (`ConfigInt`: `int8_t` to `uint64_t`, not `bool` or a character type),
  and `ReadFloat()` a pointer to a floating point type (`float` or `double`).
  That is separate from what the config holds: `ReadFloat()` reads an int in
  the config (`refresh_hz: 60`) as its float value, but `ReadInt()` never
  reads a float. An unsigned `T` rejects negative values, and a `uint64_t`
  reads only up to `INT64_MAX`, as that is all a `Config` int holds. A
  `ReadInt()` range must fit in `T` (checked with a `DCHECK`). A `float`
  rejects a value beyond its range rather than reading it as infinity.
- Enums are never read from ints (`ConfigInt` excludes them, as
  `std::integral` is false for every enum). They are read by name with
  `ReadEnum()`, which needs the valid names. Names match exactly, case
  included. An enum value may have more than one name; the error lists each
  name once, in the order given.
- After the first error, reads still read, and later errors are dropped. A
  caller can run its own checks against values read so far without first
  asking whether there was an error, and return `GetStatus()` once at the end.

## Design

### Names

- `ConfigLocation`: where a value was read from. Distinct from `gb_parse`'s
  `LexerLocation`, which is 0-based and names a lexer's content; this is
  1-based, as people read it, and has no filename (see below).
- `ConfigInternal`: the config module's access token (`gb/base/access_token.h`),
  in a new `gb/config/config_types.h`, as `MessageInternal` and
  `RenderInternal` are for theirs.
- `TextConfigBuilder`: the class in `text_config.cc` that builds a `Config`
  from parsed text, and so is a friend of `ConfigInternal`. Not
  `TextConfigReader`, which would read as a text version of `ConfigReader`, nor
  `TextConfigParser`, beside `gb_parse`'s `Parser`.
- `ConfigReader`: as in the backlog. Nothing else in `gb_config` reads into
  program settings; `ReadConfigFromText` reads text into a `Config`.
- `ConfigInt`: the concept for `ReadInt()`'s destination. Named for what
  it is in this library (an integer type a config int reads into), rather
  than a general `Integer` concept that would suggest it belongs in
  `gb_base`. Floats use the standard `std::floating_point` as is.
- `ConfigEnumName<T>`: an enum value's name in a config. Alternatives:
  `ConfigEnumValue` (reads as the value alone), `ConfigEnumEntry` (a
  catch-all). Recommend `ConfigEnumName`.
- `Reject(message)`: records an error at the value. Alternatives: `Fail`
  (reads as if the call itself failed), `AddError` ("Add" is for something
  registered and kept). Recommend `Reject`.
- `ValidateMap` / `ValidateArray`: judge the value and record an error if it
  fails, per the naming rules.

### Config locations (`gb_config`)

- `Config` gains a `ConfigLocation location_`, 8 bytes on a 40 byte value.
- Every write of the whole value goes through a private `SetValue()`, which
  clears the location, so no setter can forget to.
- `SetLocation()` takes a `ConfigInternal` token, so only the text reader (and
  the tests, through a `ConfigLocationTest` fixture) can give a value a
  location. Any other reader added to the module becomes a friend of it.
- The location has no filename: storing one per value would copy a string into
  every value, or a shared pointer that every copy of a `Config` must count.
  The program reading the file knows its name, and passes it to the reader.
- `ReadConfigFromText` looks up each item's first token with
  `Lexer::GetTokenLocation()` in `GetParsedConfig()`, adding 1 to its line and
  column. The text is added to the lexer without a filename, as every location
  lookup would copy it (a heap allocation per value for a real path). The
  filename goes into the `ParseError`'s location only when there is an error.
- The backlog put locations behind a flag. This records them always: the cost
  is a lookup per value, small beside the parse itself, and a flag is one more
  thing a program must remember to get errors that point at the text.

### ConfigReader (`gb_config`)

- In `gb/config/config_reader.h`, beside `Config`. It only needs `Config` and
  Abseil. It formats errors itself, in `ParseError`'s form, rather than
  through `ParseError`, which would mean turning 1-based locations back into
  0-based ones.
- A reader is a `const Config*`, its path as a `std::string`, the location to
  report for it, and a `std::shared_ptr` to the read's state (the source name
  and the first error). Copying one copies its path and counts the pointer.
- `Get()` builds the child's path. Reading an array of thousands of values
  (OZ-3's screen character sets) builds a path for each, a few thousand short
  strings, which is well under a millisecond and only paid when a config file
  is read.
- Thread-compatible, like `Config`. The status its const functions record is
  shared by every reader of a read, so a mutex guards it, and readers of one
  read can be used on different threads at once.

**Brittleness:**
- The `Config` must outlive every reader, as for any view. Readers are meant
  to live on the stack while a function reads.
- The caller must return `GetStatus()`. Every reader shares it, so it can be
  asked of any of them, once, at the end.
- Unknown keys are only rejected where the caller calls `ValidateMap()`. A map
  read only with `Get()` takes any keys. Calling `ValidateMap()` for each map
  is the pattern every example shows; the alternative, tracking which keys were
  read and rejecting the rest at the end, needs a step the caller can forget
  just as easily, and fails later than the problem.

### To confirm

- Nothing yet.

## CLs

### CL1 [x] config: value locations

Depends on: nothing.

- `ConfigLocation`, and `Config::GetLocation()` / `SetLocation()`, with the
  setters that replace a value clearing it.
- `config_types.h` with the `ConfigInternal` token, which `SetLocation()`
  takes.
- `ReadConfigFromText` records each value's location, and takes an optional
  `filename` for syntax errors.
- Only syntax errors change, and only for a caller that passes a filename.

**Verify**
- Standard checks (see CLAUDE.md).
- Unit tests: a new value has no location; copies and moves keep it; each
  replacing setter clears it, while setting a key or element of an existing
  map or array keeps the container's. Text locations of scalars, maps, arrays,
  nested values, values after comments and on later lines, rooted and rootless
  roots. A syntax error names the filename when one is given.

### CL2 [x] config: ConfigReader

Depends on: CL1.

- `gb/config/config_reader.h` / `.cc` / `_test.cc`.
- Unused, so no visible change.

**Verify**
- Standard checks.
- Unit tests: each read of each type, in and out of range, missing (keeps the
  default), and of the wrong type; `ReadInt` limits for small and unsigned
  types; floats read from ints, and ints never read from floats; `ReadFloat`
  into a `float` (a value too big for it is rejected); `ReadEnum` by each name,
  an unknown name or non-string (listing the names), and a missing value; `ValidateMap` names the first unknown key in sorted order;
  `ValidateArray` sizes; `Get()` on a non-map or non-array; paths through maps
  and arrays; only the first error kept, shared across copies and children;
  error text with and without a source and a location, at the root, and for a
  missing value (its map's location). One test reads a config from text end to
  end, checking an error's line and column.
