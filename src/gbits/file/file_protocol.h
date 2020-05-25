#ifndef GBITS_FILE_FILE_PROTOCOL_H_
#define GBITS_FILE_FILE_PROTOCOL_H_

#include <string>
#include <string_view>
#include <vector>

#include "gbits/file/file_types.h"

namespace gb {

// This abstract class represents a file protocol which can be registered with a
// FileSystem.
//
// The FileSystem ensures that all requests made to a FileProtocol are valid
// based on the capabilities of the protocol, so additional checking for those
// preconditions is not necessary.
//
// Further, derived classes may override the Basic* versions of the functions,
// which relegates the vast majority of the precondition checks to the default
// behavior (at the potential loss of performance). Of course, simultaneous
// interfering access from multiple threads or external processes may still be
// possible, making the precondition checks unreliable, depending on the
// environment. See each function for the precondition guarantees provided to
// each.
//
// All paths passed to a file protocol are absolute paths (start with / or //),
// and do not contain a protocol prefix (this is passed as a separate argument
// in all cases).
//
// This class has no state of its own and its thread-safety is the same as
// whatever the derived protocol class guarantees. However, see caution above if
// a protocol wishes to both be thread-safe and implement one or more Basic*
// functions *instead* of the corresponding public function (essentially,
// preconditions cannot be guaranteed for Basic* functions in this case).
class FileProtocol {
 public:
  // Copy buffer size used to implement BasicCopyFile.
  inline static constexpr int64_t kBasicCopyBufferSize = 32 * 1024;

  FileProtocol(const FileProtocol&) = delete;
  FileProtocol& operator=(const FileProtocol&) = delete;
  virtual ~FileProtocol() = default;

  // Returns the supported flags for this protocol.
  //
  // Depending on what flags are returned, one or more operations in this
  // interface must be implemented as described below.
  virtual FileProtocolFlags GetFlags() const = 0;

  // Returns the default protocol names (if any) for this protocol.
  //
  // If not overridden, the protocol contains no default names.
  virtual std::vector<std::string> GetDefaultNames() const;

  // Returns the path information for the specified path.
  //
  // Derived classes must override DoGetPathInfo to implement this if
  // FileProtocolFlag::kInfo is supported. Further, all default implementations
  // of the Do* functions require support for this regardless of file protocol
  // flags.
  PathInfo GetPathInfo(std::string_view protocol_name, std::string_view path);

  // Lists paths that match a pattern.
  //
  // Derived classes must override DoList or BasicList to implement this if
  // FileProtocolFlag::kList is supported. GetPathInfo is also called if DoList
  // is not implemented.
  std::vector<std::string> List(std::string_view protocol_name,
                                std::string_view path, std::string_view pattern,
                                FolderMode mode, PathTypes types);

  // Creates a folder given at the specified path.
  //
  // Derived classes must override DoCreateFolder or BasicCreateFolder to
  // implement this if FileProtocolFlag::kFolderCreate is supported. GetPathInfo
  // is also called if DoCreateFolder is not implemented.
  bool CreateFolder(std::string_view protocol_name, std::string_view path,
                    FolderMode mode);

  // Copies a folder from one path to another.
  //
  // Derived classes may optionally override DoCopyFolder to implement this if
  // FileProtocolFlag::kFolderCreate is supported. GetPathInfo, List,
  // CreateFolder, and CopyFile are also called if DoCopyFolder is not
  // implemented.
  bool CopyFolder(std::string_view protocol_name, std::string_view from_path,
                  std::string_view to_path);

  // Deletes a folder at the specified path.
  //
  // Derived classes must override DoDeleteFolder or BasicCopyFolder to
  // implement this if FileProtocolFlag::kFolderCreate is supported.
  // GetPathInfo, List, and DeleteFile are also called if DoDeleteFolder is not
  // implemented.
  bool DeleteFolder(std::string_view protocol_name, std::string_view path,
                    FolderMode mode);

  // Copies a file from one path to another.
  //
  // Derived classes may optionally override DoCopyFile or BasicCopyFile to
  // implement this if FileProtocolFlag::kFileCreate is supported. GetPathInfo
  // is called if DoCopyFile is not implemented. OpenFile is called if neither
  // DoCopyFile or BasicCopyFile are implemented.
  bool CopyFile(std::string_view protocol_name, std::string_view from_path,
                std::string_view to_path);

  // Deletes a file at the specified path
  //
  // Derived classes must override DoDeleteFile or BasicDeleteFile to implement
  // this if FileProtocolFlag::kFileCreate is supported. GetPathInfo is called
  // if DoDeleteFile is not implemented.
  bool DeleteFile(std::string_view protocol_name, std::string_view path);

  // Opens a file for at the specified path.
  //
  // Derived classes must override DoOpenFile or BasicOpenFile to implement
  // this. GetPathInfo is called if DoOpenFile is not implemented.
  std::unique_ptr<RawFile> OpenFile(std::string_view protocol_name,
                                    std::string_view path, FileFlags flags);

  // Describes the nature of the operation taking place for the purpose of
  // derived classes that can support atomic operations and thread-safety.
  //
  // This is public for use in tests only.
  enum class LockType {
    kInvalid,  // Never used. Placeholder for an uninitialized value.

    // In order from least restrictive to most restrictive. Each type implies
    // all access from the previous type(s).
    kQuery,      // Query properties and presence of files/folders.
    kOpenRead,   // Open an existing file for reading.
    kOpenWrite,  // Open an existing file for writing.
    kModify,     // Add or remove files/folders.
  };

 protected:
  FileProtocol() = default;

  // Lock/Unlock are called when public operations are performed on the file
  // protocol.
  //
  // If a protocol may be used in a multi-threaded context and are relying on
  // one or more default Do* method implementations, they should implement these
  // or the preconditions guaranteed to the Basic* method implementations cannot
  // be met. They also may be implemented regardless, to ensure thread-safety
  // guarantees.
  //
  // Lock/Unlock are always called in pairs, and are never nested/recursive
  // (multiple Lock calls).
  virtual void Lock(LockType type);
  virtual void Unlock(LockType type);

  // Returns the path information for the specified path.
  //
  // A protocol must implement this if the protocol supports
  // FileProtocolFlag::kInfo (which most should try to do). If implemented, it
  // must return PathInfo{PathType::kFolder} for any valid root path (generally
  // the path "/").
  //
  // If the path is inaccessible, this should return {}.
  virtual PathInfo DoGetPathInfo(std::string_view protocol_name,
                                 std::string_view path);

  // Lists paths that match a pattern.
  //
  // A protocol must implement this or BasicList if FileProtocolFlag::kList is
  // supported. An empty pattern implicitly matches all folder entries, and
  // should behave identically to passing "*" for the pattern.
  //
  // List is only called if the protocol supports FileProtocolFlag::kList. The
  // protocol is responsible for all further validation.
  //
  // The returned paths must be prefixed with the protocol prefix using the
  // provided protocol name. The special folders "." and ".." should never
  // included in the results.
  virtual std::vector<std::string> DoList(std::string_view protocol_name,
                                          std::string_view path,
                                          std::string_view pattern,
                                          FolderMode mode, PathTypes types);

  // Creates a folder given at the specified path.
  //
  // A protocol must implement this or BasicCreateFolder if
  // FileProtocolFlag::kFolderCreate is supported. If the folder already exists,
  // this should return true. If folder mode is recursive, it must recursively
  // create any missing parent folders as well.
  //
  // CreateFolder is only called if the protocol supports
  // FileProtocolFlag::kFolderCreate.
  virtual bool DoCreateFolder(std::string_view protocol_name,
                              std::string_view path, FolderMode mode);

  // Copies a folder from one path to another.
  //
  // A protocol may implement this or BasicCopyFile if
  // FileProtocolFlag::kFolderCreate is supported. It is not necessary for a
  // protocol to implement either this however, as a default implementation
  // exists.
  //
  // CopyFolder should create or replace any existing files, including
  // sub-folders and their files and folders. It is not valid to copy a file
  // onto a folder or vice versa, and this should return false if that occurs.
  // It is also should return false if the parent of 'to_path' is not the root
  // or an existing folder.
  //
  // CreateFolder is only called if the protocol supports both
  // FileProtocolFlag::kFolderCreate and FileProtocolFlag::kFileCreate, and
  // from_path is not a parent of to_path. The protocol is responsible for all
  // further validation.
  virtual bool DoCopyFolder(std::string_view protocol_name,
                            std::string_view from_path,
                            std::string_view to_path);

  // Deletes a folder at the specified path.
  //
  // A protocol must implement this or BasicDeleteFolder if
  // FileProtocolFlag::kFolderCreate is supported.
  //
  // If mode is FolderMode::kRecursive, then all files and subfolder also should
  // be deleted. If mode is FolderMode::kNormal, then this only should delete
  // the folder if it is empty. If only a partial delete is done, this should
  // return false. If the path does not exist, this should return true.
  //
  // DeleteFolder is only called is the protocol supports
  // FileProtocolFlag::kFolderCreate. The protocol is responsible for all
  // further validation.
  virtual bool DoDeleteFolder(std::string_view protocol_name,
                              std::string_view path, FolderMode mode);

  // Copies a file from one path to another.
  //
  // A protocol may implement this or BasicCopyFile if
  // FileProtocolFlag::kFileCreate is supported. It is not necessary for a
  // protocol to implement either one however, as a default implementation
  // exists.
  //
  // Copy file should replace any existing file at to_path, but should fail if
  // to_path refers to a folder, or the parent of to_path is not the root or an
  // existing folder.
  //
  // CopyFile is only called if FileProtocolFlag::kFileCreate is supported. The
  // protocol is responsible for all further validation.
  virtual bool DoCopyFile(std::string_view protocol_name,
                          std::string_view from_path, std::string_view to_path);

  // Deletes a file at the specified path
  //
  // A protocol must implement this or BasicDeleteFile if
  // FileProtocolFlag::kFileCreate is supported.
  //
  // DeleteFile should succeed if the file could successfully be delete or there
  // is no file or folder and the specified path.
  //
  // DeleteFile is only called if FileProtocolFlag::kFileCreate is supported.
  // The protocol is responsible for all further validation.
  virtual bool DoDeleteFile(std::string_view protocol_name,
                            std::string_view path);

  // Opens a file for at the specified path.
  //
  // A protocol must implement this or BasicOpenFile.
  //
  // OpenFile will only be called with FileFlags that match the corresponding
  // FileProtocolFlags supported by this protocol. FileFlag::kReset will only
  // occur in combination with FileFlag::kWrite. FileFlag::kRead and/or
  // FileFlag::kWrite will always be present. The protocol is responsible for
  // all further validation.
  //
  // On error, this should return a nullptr.
  virtual std::unique_ptr<RawFile> DoOpenFile(std::string_view protocol_name,
                                              std::string_view path,
                                              FileFlags flags);

  // File protocols may optionally override BasicList instead of List when
  // both FileProtocolFlag::kList and FileProtocolFlag::kInfo are supported.
  //
  // This function should return all files and folders directly within the
  // specified folder. This must not do any recursion or filtering of the
  // resulting files or folders.
  //
  // BasicList is only called on an existing folder, which may or may not
  // contain files and subfolders.
  //
  // The returned paths must be prefixed with the protocol prefix using the
  // provided protocol name. The special folders "." and ".." should never
  // included in the results.
  virtual std::vector<std::string> BasicList(std::string_view protocol_name,
                                             std::string_view path);

  // File protocols may optionally override BasicCreateFolder instead of
  // CreateFolder when both FileProtocolFlag::kFolderCreate and
  // FileProtocolFlag::kInfo are supported.
  //
  // This function should create the new folder and return true on success.
  //
  // BasicCreateFolder is only called on paths that are inaccessible (currently
  // invalid) and where the parent folder already exists.
  virtual bool BasicCreateFolder(std::string_view protocol_name,
                                 std::string_view path);

  // File protocols may optionally override BasicDeleteFolder instead of
  // Delete when both FileProtocolFlag::kFolderCreate and
  // FileProtocolFlag::kInfo are supported.
  //
  // This function should delete the empty folder return true on success.
  //
  // BasicDeleteFolder is only called existing folders which are empty.
  virtual bool BasicDeleteFolder(std::string_view protocol_name,
                                 std::string_view path);

  // File protocols may optionally override BasicCopyFile instead of CopyFile
  // when bot FileProtocolFlag::kFileCreate and FileProtocolFlag::kInfo are
  // supported. If FileProtocolFlag::kInfo is supported, it is not required to
  // implement either, however, as a valid default implementation exists.
  //
  // BasicCopyFile is only called if from_path refers to an existing file and
  // to_path is not a folder.
  virtual bool BasicCopyFile(std::string_view protocol_name,
                             std::string_view from_path,
                             std::string_view to_path);

  // File protocols may optionally override BasicDeleteFile instead of
  // DeleteFile when FileProtocolFlag::kFileCreate is supported.
  //
  // BasicDeleteFile is only called on existing files.
  virtual bool BasicDeleteFile(std::string_view protocol_name,
                               std::string_view path);

  // File protocols may optionally override BasicOpenFile instead of OpenFile if
  // the FileProtocolFlag::kInfo flag is supported.
  //
  // BasicOpenFile is only called on existing files if FileFlag::kFileCreate is
  // not specified. Otherwise, it may be called on inaccessible paths (currently
  // invalid) where the parent folder already exists. FileFlags will
  // occur only if the corresponding FileProtocolFlags are supported.
  // FileFlag::kReset will only occur if the protocol supports kFileWrite.
  //
  // On error, this should return a nullptr.
  virtual std::unique_ptr<RawFile> BasicOpenFile(std::string_view protocol_name,
                                                 std::string_view path,
                                                 FileFlags flags);
};

}  // namespace gb

#endif  // GBITS_FILE_FILE_PROTOCOL_H_
