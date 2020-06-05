#ifndef GBITS_FILE_FILE_SYSTEM_H_
#define GBITS_FILE_FILE_SYSTEM_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gbits/file/file.h"
#include "gbits/file/file_types.h"

namespace gb {

// This class supports a general filesystem interface implemented by one or more
// specific file protocols.
//
// File protocols are not required to support all operations. If an operation is
// not supported by a protocol, it will return a failure.
//
// This class is thread-compatible generally, and thread-safe if protocols are
// not registered or changed during execution. Thread-safety for a specific file
// operation, however, is further dependent on the file protocol used.
class FileSystem final {
 public:
  FileSystem();
  FileSystem(const FileSystem&) = delete;
  FileSystem& operator=(const FileSystem&) = delete;
  ~FileSystem();

  // Registers a protocol using its default protocol name(s).
  //
  // The protocol must have at least one default protocol name. This will
  // replace any protocol(s) previously registered against the same name(s).
  // If the protocol is null, or no the protocol does not define any protocol
  // names, or any of the protocol names are invalid, this will return false and
  // no protocol will be registered.
  bool Register(std::unique_ptr<FileProtocol> protocol);

  // Registers a protocol with the filesystem against the specified protocol
  // name.
  //
  // This will replace any protocol previously registered against the same name.
  // If the protocol is null or the protocol name is invalid, this will return
  // false and no protocol will be registered.
  bool Register(std::unique_ptr<FileProtocol> protocol,
                const std::string& protocol_name);

  // Registers a protocol with the filesystem against the specified protocol
  // names.
  //
  // There must be at least one protocol name specified, and each name must
  // non-empty, consisting of ASCII alpha-numeric characters only. This will
  // replace any protocol(s) previously registered against the same name(s).
  // If the protocol is null, or no protocol names were specified, or any
  // protocol names are invalid, this will return false and no protocol will be
  // registered.
  bool Register(std::unique_ptr<FileProtocol> protocol,
                std::vector<std::string> protocol_names);

  // Returns true if the protocol is registered.
  bool IsRegistered(const std::string& protocol_name) const;

  // Returns all registered protocol names.
  std::vector<std::string> GetProtocolNames() const;

  // Sets which named protocol should also be the default protocol.
  //
  // This will replace any existing default protocol. Returns true if the
  // default protocol was successfully set, or false if the requested protocol
  // is not registered.
  bool SetDefaultProtocol(const std::string& protocol_name);

  // Returns the name of the currently set default protocol, or empty string if
  // no default protocol was set.
  const std::string& GetDefaultProtocolName() const;

  // Returns the protocol flags for the specified protocol.
  //
  // If no protocol is registered with that name, this will return an empty set
  // of flags.
  FileProtocolFlags GetFlags(const std::string& protocol_name) const;

  // Lists all files and folders matching the pattern.
  //
  // If the pattern is empty, this will return all matching paths. If a pattern
  // is specified, any '*' in the pattern is treated as zero or more path
  // characters.
  //
  // If the folder mode is recursive and 'path' is a folder, then all
  // sub-folders of 'path' will also be listed.
  //
  // Returns a list of full paths (including protocol prefix) that are valid and
  // match.
  //
  // ListFolders and ListFiles operate thge same way, but are restricted to
  // returning folders or files respectively.
  std::vector<std::string> List(std::string_view path,
                                std::string_view pattern = {},
                                FolderMode mode = FolderMode::kNormal);
  std::vector<std::string> List(std::string_view path, FolderMode mode) {
    return List(path, {}, mode);
  }
  std::vector<std::string> ListFolders(std::string_view path,
                                       std::string_view pattern = {},
                                       FolderMode mode = FolderMode::kNormal);
  std::vector<std::string> ListFolders(std::string_view path, FolderMode mode) {
    return ListFolders(path, {}, mode);
  }
  std::vector<std::string> ListFiles(std::string_view path,
                                     std::string_view pattern = {},
                                     FolderMode mode = FolderMode::kNormal);
  std::vector<std::string> ListFiles(std::string_view path, FolderMode mode) {
    return ListFiles(path, {}, mode);
  }

  // Creates a folder.
  //
  // If mode is FolderMode::kRecursive, this will attempt to create any missing
  // parent folders as well. Returns true if completely successful, or if the
  // folder already exists. In the case of partial success (some parent folders
  // were created, but not all), this will still return false. If the folder
  // already exists, this will return true.
  bool CreateFolder(std::string_view path,
                    FolderMode mode = FolderMode::kNormal);

  // Deletes a folder.
  //
  // If mode is FolderMode::kRecursive, then this will delete all files and
  // folders underneath 'path' as well. If the the mode is not recursive and the
  // folder is not empty, this will fail and return false. If the path does not
  // exist at all (is not a file or folder), this will return true. This will
  // fail if the path refers to a file, or the deletion failed.
  bool DeleteFolder(std::string_view path,
                    FolderMode mode = FolderMode::kNormal);

  // Deletes a file.
  //
  // If the path does not exist at all (is not a file or folder), this will
  // return true. This will fail if the path refers to a folder, or the deletion
  // failed.
  bool DeleteFile(std::string_view path);

  // Copies contents of one folder to another folder.
  //
  // This will always copy the entire subfolder and file hierarchy. It will copy
  // over any existing files in the destination. Copying files over folders and
  // vice versa is not supported.
  //
  // If the copy failed or only partially succeeded, this will return false.
  bool CopyFolder(std::string_view from_path, std::string_view to_path);

  // Copies a file to a new path.
  //
  // This will replace any existing file that may be in the new path with the
  // source file. Attempting to copy a file onto a folder will always fail (to
  // copy a file into a folder, use JoinPath(to_path, RemoveFolder(from_path))
  // to create the correct destination).
  bool CopyFile(std::string_view from_path, std::string_view to_path);

  // Returns true if the path exists and is accessible (folder can be
  // listed/deleted and file can be opened/deleted).
  //
  // IsValidFolder and IsValidFile operate the same way but will return false if
  // the path is to the wrong type.
  bool IsValidPath(std::string_view path) {
    return GetPathInfo(path).type != PathType::kInvalid;
  }
  bool IsValidFolder(std::string_view path) {
    return GetPathInfo(path).type == PathType::kFolder;
  }
  bool IsValidFile(std::string_view path) {
    return GetPathInfo(path).type == PathType::kFile;
  }

  // Returns information about the specified path.
  //
  // If the path cannot be queried or does not refer to an existing file/folder,
  // the PathInfo type will be PathType::kInvalid.
  //
  // No assumption should be made that a successful file system operation will
  // necessarily result in the expected GetPathInfo response. For instance, a
  // protocol that supports folders only virtually (their existence depends on
  // if they contain files) may choose to always succeed on a CreateFolder
  // call, but may return an invalid path if the empty folder is queried.
  PathInfo GetPathInfo(std::string_view path);

  // Opens a file given a path and specified file flags.
  //
  // It is undefined behavior if a file is opened more than once. It is also
  // undefined behavior if the file is deleted, or is the source or destination
  // of a copy operation while a file is open.
  //
  // If the file cannot be opened with the requested flags, this will return
  // nullptr.
  std::unique_ptr<File> OpenFile(std::string_view path, FileFlags flags);

  // Writes a file from a string or vector of trivially copyable types.
  //
  // These are convenience wrappers for opening a file, writing the buffer, and
  // closing the file. If a file already exists, it will be overwritten.
  //
  // Returns true if the file was written successfullt, or false if any part of
  // the process failed. This may result in a partially created file.
  bool WriteFile(std::string_view path, std::string_view buffer);
  template <typename Type>
  bool WriteFile(std::string_view path, const std::vector<Type>& buffer);
  bool WriteFile(std::string_view path, const void* buffer,
                 int64_t buffer_size);

  // Reads a file into a string or vector of trivially copyable types.
  bool ReadFile(std::string_view path, std::string* buffer);
  template <typename Type>
  bool ReadFile(std::string_view path, std::vector<Type>* buffer);

  // Number of bytes copied at a time when copying files across protocols.
  inline static constexpr int64_t kCopyBufferSize = 32 * 1024;

 private:
  using Protocols = std::vector<std::unique_ptr<FileProtocol>>;
  using ProtocolMap = absl::flat_hash_map<std::string, FileProtocol*>;

  std::tuple<std::string_view, FileProtocol*> GetProtocol(
      std::string_view* path);
  bool GenericCopyFolder(std::string_view from_protocol_name,
                         FileProtocol* from_protocol,
                         std::string_view from_path,
                         std::string_view to_protocol_name,
                         FileProtocol* to_protocol, std::string_view to_path);
  bool GenericCopyFile(std::string_view from_protocol_name,
                       FileProtocol* from_protocol, std::string_view from_path,
                       std::string_view to_protocol_name,
                       FileProtocol* to_protocol, std::string_view to_path);

  Protocols protocols_;
  ProtocolMap protocol_map_;
  FileProtocol* default_protocol_ = nullptr;
  std::string default_protocol_name_;
};

inline bool FileSystem::WriteFile(std::string_view path,
                                  std::string_view buffer) {
  auto file = OpenFile(path, kNewFileFlags);
  if (file == nullptr) {
    return false;
  }
  return file->WriteString(buffer) == static_cast<int64_t>(buffer.size());
}

template <typename Type>
inline bool FileSystem::WriteFile(std::string_view path,
                                  const std::vector<Type>& buffer) {
  auto file = OpenFile(path, kNewFileFlags);
  if (file == nullptr) {
    return false;
  }
  return file->Write(buffer) == static_cast<int64_t>(buffer.size());
}

inline bool FileSystem::WriteFile(std::string_view path, const void* buffer,
                                  int64_t buffer_size) {
  auto file = OpenFile(path, kNewFileFlags);
  if (file == nullptr) {
    return false;
  }
  return file->Write(buffer, buffer_size) == buffer_size;
}

inline bool FileSystem::ReadFile(std::string_view path, std::string* buffer) {
  auto file = OpenFile(path, kReadFileFlags);
  if (file == nullptr) {
    return false;
  }
  file->ReadRemainingString(buffer);
  return true;
}

template <typename Type>
inline bool FileSystem::ReadFile(std::string_view path,
                                 std::vector<Type>* buffer) {
  auto file = OpenFile(path, kReadFileFlags);
  if (file == nullptr) {
    return false;
  }
  file->ReadRemaining(buffer);
  return true;
}

}  // namespace gb

#endif  // GBITS_FILE_FILE_SYSTEM_H_
