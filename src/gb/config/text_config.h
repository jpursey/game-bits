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
// values. It is a super-set of JSON, with additional features that allow for
// simpler and more concise configuration files. Specifically, it supports the
// following extensions:
//   - Map keys can be unquoted C-style identifiers in addition to being
//     strings.
//   - Strings can be single-quoted in addition to double-quoted.
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
  // double-quoted, unless outputing as single quoted would result in no
  // escaping.
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
// value representations, and is rootless.
inline constexpr TextConfigFlags kDefaultTextConfigFlags = {
    TextConfigFlag::kRootless, TextConfigFlag::kIdentifiers,
    TextConfigFlag::kSingleQuotes, TextConfigFlag::kComments};

// Flags that represent strict JSON compatibility when reading and writing text
// configs.
inline constexpr TextConfigFlags kJsonTextConfigFlags = {};

// Flags that represent JavaScript compatibility when reading and writing text
// formatted like a JavaScript object. Map keys can be identifiers, strings may
// be single or double quoted, and comments are allowed, but it must have a root
// {...} structure.
inline constexpr TextConfigFlags kJavaScriptTextConfigFlags = {
    TextConfigFlag::kIdentifiers, TextConfigFlag::kSingleQuotes,
    TextConfigFlag::kComments};

//==============================================================================
// Text config I/O
//==============================================================================

// Writes the given config to text with the specified flags.
//
// If the Config is not a map this will return an empty string.
std::string WriteConfigToText(const Config& config,
                              TextConfigFlags flags = kDefaultTextConfigFlags);

// Parses the config from the specified text according to the specified flags.
//
// Returns an error if the configuration cannot be parsed. If kRootless is not
// specified, then this will only read until the trailing '}' is reached,
// ignoring any text following it. If kRootless is specified, then any
// unparseable text will be an error.
absl::StatusOr<Config> ReadConfigFromText(
    std::string text, TextConfigFlags flags = kDefaultTextConfigFlags);

}  // namespace gb

#endif  // GB_CONFIG_TEXT_CONFIG_H_