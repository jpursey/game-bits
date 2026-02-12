// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONFIG_TEXT_CONFIG_H_
#define GB_CONFIG_TEXT_CONFIG_H_

#include <string>

#include "absl/status/statusor.h"
#include "gb/base/flags.h"
#include "gb/config/config.h"

namespace gb {

//==============================================================================
// Config text file format
//
// The text config format is a human-readable format for representing Config
// values. It is mostly a super-set of JSON (with the exception of some string
// escape codes), with additional features that allow for simpler and more
// concise configuration files. Specifically, it supports the following
// extensions:
//   - Map keys can be unquoted C-style identifiers in addition to being
//     strings.
//   - Strings can be single-quoted in addition to double-quoted. Strings
//     support all valid UTF-8 characters, with non-printable ASCII characters
//     (those below 0x20) always being escaped. Quotes that match the enclosing
//     quote style also be escaped. The escape character is '\' and supports
//     '\n', '\t', and'\xNN' hex escapes. All other characters after '\' are
//     treated as the character itself (e.g. '\\' is just '\').
//   - Comments are allowed, using either // for line comments or /* */ for
//     block comments.
//   - By default, the text config format is rootless, meaning that the
//     top-level structure is a map without an enclosing {...} wrapper. This
//     allows for more concise configuration files
//
// Finally, although this does not affect the format of reading and writing
// files, Config values make a distinction between integers and floating point
// numbers, for precision and type safety. When reading text configs, the text
// config format will always read numbers without a decimal point as integers,
// and numbers with a decimal point as floating point numbers.
//==============================================================================

//==============================================================================
// Text config flags
//==============================================================================

enum class TextConfigFlag {
  // The root Config is stored without a {...} wrapper, so the text config is
  // just the contents of the map. This affects both reading and writing.
  kRootless,

  // Map keys are allowed to be identifiers in addition to strings. When
  // writing, keys will preferentially be output as an identifier if they can
  // be.
  kIdentifiers,

  // Allow single-quoted strings in the text config in addition to double-quoted
  // strings. When writing, strings will preferentially be output as
  // double-quoted, unless a string contains a double quote but not a single
  // quote, in which case it will be output as single-quoted.
  kSingleQuotes,

  // Allow // and /* */ comments in the text config. This is not compatible with
  // strict JSON, but is compatible with JavaScript and many other config
  // formats. This is only meaningful when reading, as writing will never
  // produce comments.
  kComments,

  // This enabled compact output with no unnecessary whitespace. This is only
  // meaningful when writing, as reading will ignore all whitespace.
  kCompact,
};
using TextConfigFlags = Flags<TextConfigFlag>;

// Default flags for text config reading and writing supports the most expansive
// value representations.
inline constexpr TextConfigFlags kDefaultTextConfig = {
    TextConfigFlag::kIdentifiers, TextConfigFlag::kSingleQuotes,
    TextConfigFlag::kComments};

// Flags that represent compact output when writing text configs.
inline constexpr TextConfigFlags kCompactTextConfig = {
    kDefaultTextConfig, TextConfigFlag::kCompact};

// Flags that represent rootless text configs when reading and writing text.
// This is useful for reading / writing config files.
inline constexpr TextConfigFlags kRootlessTextConfig = {
    kDefaultTextConfig, TextConfigFlag::kRootless};

// Flags that represent JSON compatibility when reading and writing text.
inline constexpr TextConfigFlags kJsonTextConfig = {};

//==============================================================================
// Text config output settings
//
// This is only relevant when writing text configs, as reading ignores all
// whitespace.
//==============================================================================

// The number of spaces to indent each level of nesting when writing text
// configs. Indentation only occurs when kCompact is not set. Lines are indented
// after new lines, with the ident level increasing only for child elements of a
// map or array. Newlines are added after a map opening and closing brace, after
// each map key/value pair, and whenever a list of array values exceeds
// kTextConfigMaxArrayLineLength.
constexpr int kTextConfigIndentSize = 2;

// The preferred maximum line length when writing text config arrays. This is
// only relevant when kCompact is not set. If multiple values in an array would
// be written on the same line and exceed this length, a newline will be
// inserted before the value.
constexpr int kTextConfigMaxArrayLineLength = 80;

//==============================================================================
// Text config I/O
//==============================================================================

// Writes the given config to text with the specified flags.
//
// If kRootless is set and the Config is not a map, then this will return an
// empty string. Otherwise, it will write the Config as requested.
std::string WriteConfigToText(const Config& config,
                              TextConfigFlags flags = kDefaultTextConfig);

// Parses the config from the specified text according to the specified flags.
//
// Returns an error if the configuration cannot be parsed. If kRootless is set,
// then the text will be read as if it is a map (key/value pairs). Otherwise it
// will read the text as any Config type (maps must start be enclosed in {...}).
// Any parse error will result in the entire config not being read, and an error
// status returned indicating the parse error.
absl::StatusOr<Config> ReadConfigFromText(
    std::string text, TextConfigFlags flags = kDefaultTextConfig);

}  // namespace gb

#endif  // GB_CONFIG_TEXT_CONFIG_H_