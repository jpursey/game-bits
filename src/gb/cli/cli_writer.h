// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_WRITER_H_
#define GB_CLI_CLI_WRITER_H_

#include <functional>
#include <string_view>

namespace gb {

//==============================================================================
// CliWriter
//==============================================================================

// What a piece of text written to a CliWriter is.
enum class CliText {
  kHelp,   // Help that was asked for, which ends with a newline.
  kError,  // What is wrong with a command line, which doesn't end with a
           // newline.
};

// Writes help to stdout, and errors to stderr followed by a newline. This is
// the default CliWriter.
void WriteCliText(CliText type, std::string_view text);

// Where a command line's text goes. By default, this is WriteCliText().
//
// A program sends the text elsewhere (a log, a window) by putting its own
// writer in the context it parses the command line with:
// `context.SetValue<CliWriter>(WriteToLog)`.
using CliWriter = std::function<void(CliText type, std::string_view text)>;

}  // namespace gb

#endif  // GB_CLI_CLI_WRITER_H_
