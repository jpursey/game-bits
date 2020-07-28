// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_TEST_PROTOCOL_H_
#define GB_FILE_TEST_PROTOCOL_H_

#include <map>
#include <memory>
#include <vector>

#include "gb/file/file_protocol.h"

namespace gb {

// This class is a trivial implementation of FileProtocol which tracks all
// interaction with the protocol. This is used by file system tests to verify
// the behavior of FileProtocol, FileSystem, and File.
class TestProtocol : public FileProtocol {
 public:
  // Internal tracking state for a file.
  struct FileState {
    explicit FileState(std::string contents) : contents(std::move(contents)) {}

    void ResetCounts();

    RawFile* file = nullptr;  // Open file, if there is one.
    FileFlags flags;          // Flags the file was created with.
    int64_t position = 0;     // Current file position. Set to -1 to make
                              // the file invalid, causing all RawFile methods
                              // to fail.
    std::string contents;     // Contents of the file.

    // Seek will fail.
    bool fail_seek = false;

    // If non-negative, write will fail after this many total bytes are
    // requested to be written.
    int64_t fail_write_after = -1;

    // If non-negative, read will fail after this many total bytes are requested
    // to be read.
    int64_t fail_read_after = -1;

    // The following counts are reset with ResetCounts().
    int invalid_call_count = 0;         // Counts code paths that should never
                                        // happen when used with a FileSystem.
    int seek_end_count = 0;             // Counts calls to SeekEnd.
    int seek_to_count = 0;              // Counts calls to SeekTo.
    int write_count = 0;                // Counts calls to Write.
    int read_count = 0;                 // Counts calls to Read.
    int64_t request_bytes_written = 0;  // Total bytes requested from Write.
    int64_t bytes_written = 0;          // Total bytes actually written.
    int64_t request_bytes_read = 0;     // Total bytes requested from Read.
    int64_t bytes_read = 0;             // Total bytes actually read.
  };

  // Internal tracking state for a path accessed via the protocol.
  class PathState {
   public:
    PathState() = default;
    PathState(PathState&&) = default;
    PathState& operator=(PathState&&) = default;
    ~PathState() = default;

    static PathState NewFolder() {
      return PathState(PathType::kFolder, nullptr);
    }
    static PathState NewFile(std::string contents = {}) {
      return PathState(PathType::kFile,
                       std::make_unique<FileState>(std::move(contents)));
    }

    PathType GetType() const { return type_; }
    int64_t GetSize() const {
      return (file_ != nullptr ? static_cast<int64_t>(file_->contents.size())
                               : 0);
    }
    std::string GetContents() const {
      return (file_ != nullptr ? file_->contents : std::string{});
    }
    void SetContents(std::string_view contents) {
      if (file_ != nullptr) {
        file_->contents.assign(contents.data(), contents.size());
      }
    }
    FileState* GetFile() const { return file_.get(); }

   private:
    PathState(PathType type, std::unique_ptr<FileState> file)
        : type_(type), file_(std::move(file)) {}
    PathState(FileFlags flags, std::string contents)
        : type_(PathType::kFile), file_() {}

    PathType type_ = PathType::kInvalid;
    std::unique_ptr<FileState> file_;
  };
  using PathStates = std::map<std::string, PathState>;

  // Internal tracking state for the entire protocol.
  struct State {
    State() = default;

    void ResetCounts();
    void ResetState();

    TestProtocol* protocol = nullptr;  // Non-null when part of a protocol.

    FileProtocolFlags flags = kAllFileProtocolFlags;
    std::string name;  // Name expected when matching calls. Empty accepts any.
    std::vector<std::string>
        default_names;  // Default names, for auto-registration.
    bool implement_copy = false;
    bool delete_state = false;

    // Current lock type
    LockType lock_type = LockType::kInvalid;

    // If an operation attempts to use this path, it will fail.
    std::string fail_path;

    // If an open operation attempts to use this path, it will fail.
    std::string open_fail_path;

    // If a file read/write attempts to use this path, it will fail.
    std::string io_fail_path;

    // The following counts are reset with ResetCounts().
    int invalid_call_count = 0;   // Counts code paths that should never happen.
                                  // when used with a FileSystem.
    int list_count = 0;           // Counts calls to List.
    int create_folder_count = 0;  // Counts calls to CreateFolder.
    int delete_folder_count = 0;  // Counts calls to DeleteFolder.
    int delete_file_count = 0;    // Counts calls to DeleteFile.
    int copy_folder_count = 0;    // Counts calls to CopyFolder.
    int copy_file_count = 0;      // Counts calls to CopyFile.
    int open_file_count = 0;      // Counts calls to OpenFile.
    int basic_list_count = 0;     // Counts calls to BasicList.
    int basic_create_folder_count = 0;  // Counts calls to BasicCreateFolder.
    int basic_delete_folder_count = 0;  // Counts calls to BasicDeleteFolder.
    int basic_copy_file_count = 0;      // Counts calls to BasicCopyFile.
    int basic_delete_file_count = 0;    // Counts calls to BasicDeleteFile.
    int basic_open_file_count = 0;      // Counts calls to BasicOpenFile.

    // Paths that were accessed through the protocol implicitly or explicitly.
    // Paths are never removed during FileProtocol operations, but become
    // kInvalid to indicate the path was accessed, but there is no file or
    // folder at that path. Paths are completely removed if ResetState() is
    // called.
    PathStates paths;
  };

  TestProtocol(State* state) : state_(state) { state_->protocol = this; }
  ~TestProtocol() override {
    state_->protocol = nullptr;
    if (state_->delete_state) {
      delete state_;
    }
  }

  // Implementation for FileProtocol.
  FileProtocolFlags GetFlags() const override;
  std::vector<std::string> GetDefaultNames() const override;

  // These are deliberately public, so they can be called directly from tests.
  void Lock(LockType type) override;
  void Unlock(LockType type) override;
  PathInfo DoGetPathInfo(std::string_view protocol_name,
                         std::string_view path) override;
  std::vector<std::string> DoList(std::string_view protocol_name,
                                  std::string_view path,
                                  std::string_view pattern, FolderMode mode,
                                  PathTypes types) override;
  bool DoCreateFolder(std::string_view protocol_name, std::string_view path,
                      FolderMode mode) override;
  bool DoDeleteFolder(std::string_view protocol_name, std::string_view path,
                      FolderMode mode) override;
  bool DoCopyFolder(std::string_view protocol_name, std::string_view from_path,
                    std::string_view to_path) override;
  bool DoCopyFile(std::string_view protocol_name, std::string_view from_path,
                  std::string_view to_path) override;
  bool DoDeleteFile(std::string_view protocol_name,
                    std::string_view path) override;
  std::unique_ptr<RawFile> DoOpenFile(std::string_view protocol_name,
                                      std::string_view path,
                                      FileFlags flags) override;
  std::vector<std::string> BasicList(std::string_view protocol_name,
                                     std::string_view path) override;
  bool BasicCreateFolder(std::string_view protocol_name,
                         std::string_view path) override;
  bool BasicDeleteFolder(std::string_view protocol_name,
                         std::string_view path) override;
  bool BasicCopyFile(std::string_view protocol_name, std::string_view from_path,
                     std::string_view to_path) override;
  bool BasicDeleteFile(std::string_view protocol_name,
                       std::string_view path) override;
  std::unique_ptr<RawFile> BasicOpenFile(std::string_view protocol_name,
                                         std::string_view path,
                                         FileFlags flags) override;

 private:
  bool IsValidProtocolName(std::string_view protocol_name) const {
    return state_->name.empty() || protocol_name == state_->name;
  }
  bool IsValidPath(std::string_view path) const {
    return !path.empty() && path[0] == '/';
  }

  State* state_;
};

}  // namespace gb

#endif  // GB_FILE_TEST_PROTOCOL_H_
