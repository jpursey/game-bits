// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_MEMORY_FILE_PROTOCOL_H_
#define GB_FILE_MEMORY_FILE_PROTOCOL_H_

#include <map>
#include <memory>

#include "absl/synchronization/mutex.h"
#include "gb/base/string_util.h"
#include "gb/file/file_protocol.h"

namespace gb {

// This class implements a file protocol interface using heap memory.
//
// This class supports all file system operations, allocating memory as needed
// from the heap. By default, it will register under the "mem" protocol name.
//
// This class is thread-safe.
class MemoryFileProtocol : public FileProtocol {
 public:
  explicit MemoryFileProtocol(FileProtocolFlags flags = kAllFileProtocolFlags);
  ~MemoryFileProtocol() override;

  // Public overrides for FileProtocol.
  FileProtocolFlags GetFlags() const override;
  std::vector<std::string> GetDefaultNames() const override;

 protected:
  // Protected overrides for FileProtocol.
  void Lock(LockType type) override;
  void Unlock(LockType type) override;
  PathInfo DoGetPathInfo(std::string_view protocol_name,
                         std::string_view path) override;
  std::vector<std::string> BasicList(std::string_view protocol_name,
                                     std::string_view path) override;
  bool BasicCreateFolder(std::string_view protocol_name,
                         std::string_view path) override;
  bool BasicDeleteFolder(std::string_view protocol_name,
                         std::string_view path) override;
  bool BasicDeleteFile(std::string_view protocol_name,
                       std::string_view path) override;
  std::unique_ptr<RawFile> BasicOpenFile(std::string_view protocol_name,
                                         std::string_view path,
                                         FileFlags flags) override;

 private:
  class MemoryFile;
  struct Node;
  using Nodes = std::map<std::string, std::unique_ptr<Node>, StringKeyCompare>;

  const FileProtocolFlags flags_;

  absl::Mutex mutex_;
  Nodes nodes_;
};

}  // namespace gb

#endif  // GB_FILE_MEMORY_FILE_PROTOCOL_H_
