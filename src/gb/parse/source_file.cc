// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/source_file.h"

#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"

namespace gb {

std::unique_ptr<SourceFile> SourceFile::FromFileText(std::string_view filename,
                                                     std::string content) {
  return absl::WrapUnique(new SourceFile(filename, std::move(content)));
}

std::unique_ptr<SourceFile> SourceFile::FromText(std::string content) {
  return absl::WrapUnique(new SourceFile("", content));
}

SourceFile::SourceFile(std::string_view filename, std::string content)
    : filename_(filename), content_(std::move(content)) {
  lines_ = absl::StrSplit(content_, '\n');
  if (lines_.back().empty()) {
    lines_.pop_back();
  }
}

}  // namespace gb
