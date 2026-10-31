// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_LOCAL_FILE_PROTOCOL_H_
#define GB_FILE_LOCAL_FILE_PROTOCOL_H_

#include <filesystem>
#include <string>

#include "gb/base/validated_context.h"
#include "gb/file/file_protocol.h"
#include "gb/file/file_types.h"

namespace gb {

// This class implements FileProtocol rooted in a folder on the local operating
// system, or at the operating system's root, to open any path in it (see
// CreateOsRoot()).
//
// This class supports all file system operations, but is subject to the
// requirements of the underlying operating system. By default, it will register
// under the "file" protocol name.
//
// Paths are UTF-8. A path with a host (such as "file://host/a") isn't under the
// root, so it is invalid.
//
// Only directories and regular files are supported. Links (symbolic links and
// junctions) are followed, so a link appears as the file or folder it links to,
// except that recursive listing doesn't descend into a linked folder, and
// deleting a link deletes only the link. Other types of files, and links whose
// target is missing, are skipped when listing, and appear invalid when queried
// with GetPathInfo(). Links and other types of files can interfere with folder
// copying and deletion.
//
// This class is thread-safe.
class LocalFileProtocol : public FileProtocol {
 public:
  static constexpr FileProtocolFlags kDefaultFlags = {
      kReadWriteFileProtocolFlags, FileProtocolFlag::kCurrentPath};

  // Flags can be set to limit what operations are allowed when this protocol is
  // added to a FileSystem. By default, all operations are supported.
  static GB_CONTEXT_CONSTRAINT_DEFAULT(kConstraintFlags, kInOptional,
                                       FileProtocolFlags, kDefaultFlags);

  // This defines the root path on the local filesystem that will be the root
  // folder for this protocol. It is normalized (see path.h for details), and
  // GetRoot() returns the normalized path. Relative paths are allowed
  // (including the empty string), which will be relative to the current working
  // directory. This must be a path to a valid folder or a new path whose parent
  // folder is a valid folder. In the latter case, a folder will be created if
  // possible.
  static inline constexpr const char* kKeyRoot = "root";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintRoot, kInRequired, std::string,
                                     kKeyRoot);

  // If this is set to true, then the root path is used to generate a new unique
  // root path as follows:
  // - If kConstraintRoot refers to an existing folder, then a randomly named
  //   folder will be created below it.
  // - If kConstraintRoot refers to a new path, that path will be used as a
  //   prefix for generating a randomly named folder.
  static inline constexpr const char* kKeyUniqueRoot = "unique_root";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintUniqueRoot, kInOptional, bool,
                                     kKeyUniqueRoot);

  // This can optionally be set to true, which will result in any files and
  // folders under the root folder to be deleted. If kConstraintUniqueRoot is
  // true, then this will also delete the root folder itself.
  static inline constexpr const char* kKeyDeleteAtExit = "delete_at_exit";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintDeleteAtExit, kInOptional, bool,
                                     kKeyDeleteAtExit);

  // Contract for creating a new LocalFileProtocol.
  using Contract =
      ContextContract<kConstraintFlags, kConstraintRoot, kConstraintUniqueRoot,
                      kConstraintDeleteAtExit>;

  // Creates a new LocalFileProtocol.
  //
  // This will fail if the contract is invalid, or if the requested root folder
  // could not be established.
  static std::unique_ptr<LocalFileProtocol> Create(Contract contract);

  // Convenience method that creates a local file protocol rooted at a folder,
  // as kConstraintRoot describes, with the defaults for everything else.
  //
  // This fails if the root path can't be converted to a UTF-8 path, as well as
  // for the reasons Create(Contract) can.
  static std::unique_ptr<LocalFileProtocol> Create(
      const std::filesystem::path& root);

  // Convenience method that creates a local file protocol to a new folder in
  // the operating system specific temp directory.
  //
  // If a prefix is specified, this will be used to during generation of the
  // root folder.
  static std::unique_ptr<LocalFileProtocol> CreateTemp(
      std::string_view temp_prefix = {});

  // Creates a local file protocol rooted at the top of the operating system's
  // file system, which opens any absolute path in it, such as the paths a
  // command line tool is given. ToOsRootPath() converts an operating system
  // path to one of its paths.
  //
  // Where the operating system has drive letters, the root is above them, and
  // the first folder in a path is the drive, in lowercase: "/c/src/a" is
  // "C:\src\a". The root itself isn't a folder, so .. segments that go above a
  // drive's root leave the drive. Network paths (such as "\\server\share")
  // can't be opened.
  static std::unique_ptr<LocalFileProtocol> CreateOsRoot();

  // Converts an operating system path, absolute or relative to the current
  // directory, to a path of a protocol from CreateOsRoot(). The path is
  // normalized and absolute, with no protocol name, so it is opened with that
  // protocol as the default protocol, or prefixed with its name.
  //
  // Returns an empty string if the path is empty, isn't UTF-8, or is a network
  // path.
  static std::string ToOsRootPath(std::string_view native_path);

  ~LocalFileProtocol() override;

  // Returns the normalized root path, or an empty string for a protocol from
  // CreateOsRoot().
  const std::string& GetRoot() const { return root_; }

  // Public overrides for FileProtocol.
  FileProtocolFlags GetFlags() const override;
  std::vector<std::string> GetDefaultNames() const override;

 protected:
  // Protected overrides for FileProtocol.
  PathInfo DoGetPathInfo(std::string_view protocol_name,
                         std::string_view path) override;
  std::vector<std::string> DoList(std::string_view protocol_name,
                                  std::string_view path,
                                  std::string_view pattern, FolderMode mode,
                                  PathTypes types) override;
  bool DoCreateFolder(std::string_view protocol_name, std::string_view path,
                      FolderMode mode) override;
  bool DoCopyFolder(std::string_view protocol_name, std::string_view from_path,
                    std::string_view to_path) override;
  bool DoDeleteFolder(std::string_view protocol_name, std::string_view path,
                      FolderMode mode) override;
  bool DoCopyFile(std::string_view protocol_name, std::string_view from_path,
                  std::string_view to_path) override;
  bool DoDeleteFile(std::string_view protocol_name,
                    std::string_view path) override;
  std::unique_ptr<RawFile> DoOpenFile(std::string_view protocol_name,
                                      std::string_view path,
                                      FileFlags flags) override;
  std::string DoGetCurrentPath(std::string_view protocol_name) override;
  bool DoSetCurrentPath(std::string_view protocol_name,
                        std::string_view path) override;

 private:
  LocalFileProtocol(FileProtocolFlags flags, std::string_view root,
                    bool unique_root, bool delete_at_exit);

  const FileProtocolFlags flags_;
  const std::string root_;
  const bool unique_root_;
  const bool delete_at_exit_;
};

}  // namespace gb

#endif  // GB_FILE_LOCAL_FILE_PROTOCOL_H_
