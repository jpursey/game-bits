// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_MEMORY_FILE_PROTOCOL_H_
#define GB_FILE_MEMORY_FILE_PROTOCOL_H_

#include <map>
#include <memory>

#include "absl/synchronization/mutex.h"
#include "gb/base/flags.h"
#include "gb/base/string_util.h"
#include "gb/file/file_protocol.h"
#include "gb/file/file_types.h"

namespace gb {

// This class implements a file protocol interface using heap memory.
//
// This class supports all file system operations, allocating memory as needed
// from the heap. By default, it will register under the "mem" protocol name.
// Paths are case-sensitive. A file that is open can't be deleted or moved, nor
// can a folder with an open file in it be moved.
//
// Files and folders can be read-only and hidden. A read-only file can't be
// opened for writing, and ReadOnlyFlags add what else it prevents. A read-only
// folder, and a hidden file or folder, prevent nothing. Attributes can be
// changed while a file is open, and the open file keeps the access it was
// opened with.
//
// This class is thread-safe.
class MemoryFileProtocol : public FileProtocol {
 public:
  static constexpr FileProtocolFlags kDefaultFlags = {
      kReadWriteFileProtocolFlags, FileProtocolFlag::kMove};

  // What a read-only file prevents, beyond opening it for writing. With none
  // set, a read-only file can be deleted.
  enum class ReadOnlyFlag {
    // A read-only file can't be deleted, including in a recursive folder
    // delete, which fails at the first read-only file, having deleted what came
    // before it.
    kPreventDelete,
  };
  using ReadOnlyFlags = Flags<ReadOnlyFlag>;

  explicit MemoryFileProtocol(FileProtocolFlags flags = kDefaultFlags,
                              ReadOnlyFlags read_only_flags = {});
  ~MemoryFileProtocol() override;

  // Public overrides for FileProtocol.
  FileProtocolFlags GetFlags() const override;
  FileAttributes GetAttributeFlags() const override;
  std::vector<std::string> GetDefaultNames() const override;

 protected:
  // Protected overrides for FileProtocol.
  void Lock(LockType type) override;
  void Unlock(LockType type) override;
  PathInfo DoGetPathInfo(std::string_view protocol_name,
                         std::string_view path) override;
  bool BasicSetAttributes(std::string_view protocol_name, std::string_view path,
                          FileAttributes attributes) override;
  std::vector<std::string> BasicList(std::string_view protocol_name,
                                     std::string_view path) override;
  bool BasicCreateFolder(std::string_view protocol_name,
                         std::string_view path) override;
  bool BasicDeleteFolder(std::string_view protocol_name,
                         std::string_view path) override;
  bool BasicDeleteFile(std::string_view protocol_name,
                       std::string_view path) override;
  bool BasicMoveFolder(std::string_view from_protocol_name,
                       std::string_view from_path,
                       std::string_view to_protocol_name,
                       std::string_view to_path) override;
  bool BasicMoveFile(std::string_view from_protocol_name,
                     std::string_view from_path,
                     std::string_view to_protocol_name,
                     std::string_view to_path) override;
  std::unique_ptr<RawFile> BasicOpenFile(std::string_view protocol_name,
                                         std::string_view path,
                                         FileFlags flags) override;

 private:
  class MemoryFile;
  struct Node;
  using Nodes = std::map<std::string, std::unique_ptr<Node>, StringKeyCompare>;

  const FileProtocolFlags flags_;
  const ReadOnlyFlags read_only_flags_;

  absl::Mutex mutex_;
  Nodes nodes_;
};

}  // namespace gb

#endif  // GB_FILE_MEMORY_FILE_PROTOCOL_H_
