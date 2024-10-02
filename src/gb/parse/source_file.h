// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/types/span.h"

namespace gb {

//------------------------------------------------------------------------------
// SourceFile
//------------------------------------------------------------------------------

// Represents a source file, with the filename and content of the file.
class SourceFile final {
 public:
  // Creates a new SourceFile associated with text from a file on disk.
  static std::unique_ptr<SourceFile> FromFileText(std::string_view filename,
                                                  std::string content);

  // Creates a new SourceFile associated with text content without any file.
  static std::unique_ptr<SourceFile> FromText(std::string content);

  // SourceFile is move-only.
  SourceFile(const SourceFile&) = delete;
  SourceFile& operator=(const SourceFile&) = delete;
  ~SourceFile() = default;

  // Returns the filename of the source file, or an empty string if there is no
  // associated file.
  const std::string_view GetFilename() const { return filename_; }

  // Returns the content of the source file.
  const std::string_view GetContent() const { return content_; }

  // Returns the content of the source file as a list of lines.
  absl::Span<const std::string_view> GetLines() const { return lines_; }

 private:
  SourceFile(std::string_view filename, std::string content);

  std::string filename_;
  std::string content_;
  std::vector<std::string_view> lines_;
};

}  // namespace gb
