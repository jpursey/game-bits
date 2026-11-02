// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_FAKE_CLI_WRITER_H_
#define GB_CLI_FAKE_CLI_WRITER_H_

#include <string>
#include <string_view>
#include <utility>

#include "absl/strings/str_cat.h"
#include "gb/cli/cli_writer.h"

namespace gb {

// Keeps what is written to the CliWriter it makes, for gb_cli's tests.
class FakeCliWriter final {
 public:
  // Returns a writer that writes to this, which must outlive it.
  CliWriter GetWriter() {
    return [this](CliText type, std::string_view text) { Write(type, text); };
  }

  // Returns the help written since the last call, and forgets it.
  std::string TakeHelp() { return std::exchange(help_, {}); }

  // Returns the errors written since the last call, and forgets them.
  std::string TakeError() { return std::exchange(error_, {}); }

 private:
  void Write(CliText type, std::string_view text) {
    absl::StrAppend(type == CliText::kHelp ? &help_ : &error_, text);
  }

  std::string help_;
  std::string error_;
};

}  // namespace gb

#endif  // GB_CLI_FAKE_CLI_WRITER_H_
