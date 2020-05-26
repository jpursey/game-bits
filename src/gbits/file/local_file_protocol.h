#ifndef GBITS_FILE_LOCAL_FILE_PROTOCOL_H_
#define GBITS_FILE_LOCAL_FILE_PROTOCOL_H_

#include <string>

#include "gbits/base/validated_context.h"
#include "gbits/file/file_protocol.h"

namespace gb {

// This class implements FileProtocol rooted in a folder on the local operating
// system.
//
// This class supports all file system operations, but is subject to the
// requirements of the underlying operating system. By default, it will register
// under the "file" protocol name.
//
// Only directories and regular files are supported. If other types of files
// are encountered (most common is a symlink), they are skipped or appear as
// invalid when queried with GetPathInfo(). The existence of these types of
// files can interfere with folder copying and deletion.
//
// This class is thread-safe.
class LocalFileProtocol : public FileProtocol {
 public:
  // Flags can be set to limit what operations are allowed when this protocol is
  // added to a FileSystem. By default, all operations are supported.
  static GB_CONTEXT_CONSTRAINT_DEFAULT(kConstraintFlags, kInOptional,
                                       FileProtocolFlags,
                                       kAllFileProtocolFlags);

  // This defines the root path on the local filesystem that will be the root
  // folder for this protocol. This should be a normalized path (see path.h for
  // details). Relative paths are allowed (including the empty string), which
  // will be relative to the current working directory. This must be a path to a
  // valid folder or a new path whose parent folder is a valid folder. In the
  // latter case, a folder will be created if possible.
  static inline constexpr char* kKeyRoot = "root";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintRoot, kInRequired, std::string,
                                     kKeyRoot);

  // If this is set to true, then the root path is used to generate a new unique
  // root path as follows:
  // - If kConstraintRoot refers to an existing folder, then a randomly named
  //   folder will be created below it.
  // - If kConstraintRoot refers to a new path, that path will be used as a
  //   prefix for generating a randomly named folder below it.
  static inline constexpr char* kKeyUniqueRoot = "unique_root";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintUniqueRoot, kInOptional, bool,
                                     kKeyUniqueRoot);

  // This can optionally be set to true, which will result in any files and
  // folders under the root folder to be deleted. If kConstraintUniqueRoot is
  // true, then this will also delete the root folder itself.
  static inline constexpr char* kKeyDeleteAtExit = "delete_at_exit";
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

  // Convenience method that creates a local file protocol to a new folder in
  // the operating system specific temp directory.
  //
  // If a prefix is specified, this will be used to during generation of the
  // root folder.
  static std::unique_ptr<LocalFileProtocol> CreateTemp(
      std::string_view temp_prefix = {});

  ~LocalFileProtocol() override;

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

 private:
  explicit LocalFileProtocol(std::string_view root,
                             const ValidatedContext& context);

  const FileProtocolFlags flags_;
  const std::string root_;
  const bool unique_root_;
  const bool delete_at_exit_;
};

}  // namespace gb

#endif  // GBITS_FILE_LOCAL_FILE_PROTOCOL_H_
