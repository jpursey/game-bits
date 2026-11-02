// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_writer.h"

#include <cstdio>

namespace gb {

namespace {

void WriteText(std::FILE* file, std::string_view text) {
  std::fwrite(text.data(), 1, text.size(), file);
}

}  // namespace

void WriteCliText(CliText type, std::string_view text) {
  switch (type) {
    case CliText::kHelp:
      WriteText(stdout, text);
      break;
    case CliText::kError:
      WriteText(stderr, text);
      WriteText(stderr, "\n");
      break;
  }
}

}  // namespace gb
