#include "gbits/file/file_system.h"

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "gbits/file/file_protocol.h"
#include "gbits/file/path.h"
#include "gbits/file/raw_file.h"

namespace gb {

namespace {

constexpr PathFlags kFileSystemPathFlags =
    kGenericPathFlags + PathFlag::kRequireRoot;

bool ValidateProtocolFlags(FileProtocolFlags flags) {
  if (flags.IsSet(FileProtocolFlag::kList) &&
      !flags.IsSet(FileProtocolFlag::kInfo)) {
    return false;
  }
  if (flags.IsSet(FileProtocolFlag::kFolderCreate) &&
      !flags.IsSet(FileProtocolFlag::kFileCreate)) {
    return false;
  }
  if (flags.IsSet(FileProtocolFlag::kFileCreate) &&
      !flags.IsSet(FileProtocolFlag::kFileWrite)) {
    return false;
  }
  if (!flags.Intersects(
          {FileProtocolFlag::kFileRead, FileProtocolFlag::kFileWrite})) {
    return false;
  }
  return true;
}

}  // namespace

FileSystem::FileSystem() {}
FileSystem::~FileSystem() {}

bool FileSystem::Register(std::unique_ptr<FileProtocol> protocol) {
  if (protocol == nullptr) {
    return false;
  }
  return Register(std::move(protocol), protocol->GetDefaultNames());
}

bool FileSystem::Register(std::unique_ptr<FileProtocol> protocol,
                          const std::string& protocol_name) {
  std::vector<std::string> protocol_names = {protocol_name};
  return Register(std::move(protocol), std::move(protocol_names));
}

bool FileSystem::Register(std::unique_ptr<FileProtocol> protocol,
                          std::vector<std::string> protocol_names) {
  if (protocol == nullptr || protocol_names.empty() ||
      !ValidateProtocolFlags(protocol->GetFlags())) {
    return false;
  }
  for (const auto& protocol_name : protocol_names) {
    if (!IsValidProtocolName(protocol_name)) {
      return false;
    }
    if (protocol_map_.find(protocol_name) != protocol_map_.end()) {
      return false;
    }
  }
  for (const auto& protocol_name : protocol_names) {
    protocol_map_[protocol_name] = protocol.get();
  }
  protocols_.emplace_back(std::move(protocol));
  return true;
}

bool FileSystem::IsRegistered(const std::string& protocol_name) const {
  return protocol_map_.find(protocol_name) != protocol_map_.end();
}

std::vector<std::string> FileSystem::GetProtocolNames() const {
  std::vector<std::string> protocol_names;
  protocol_names.reserve(protocol_map_.size());
  for (const auto& protocol : protocol_map_) {
    protocol_names.emplace_back(protocol.first);
  }
  return protocol_names;
}

bool FileSystem::SetDefaultProtocol(const std::string& protocol_name) {
  auto it = protocol_map_.find(protocol_name);
  if (it == protocol_map_.end()) {
    return false;
  }
  default_protocol_ = it->second;
  default_protocol_name_ = protocol_name;
  return true;
}

const std::string& FileSystem::GetDefaultProtocolName() const {
  return default_protocol_name_;
}

FileProtocolFlags FileSystem::GetFlags(const std::string& protocol_name) const {
  auto it = protocol_map_.find(protocol_name);
  if (it == protocol_map_.end()) {
    return {};
  }
  return it->second->GetFlags();
}

std::vector<std::string> FileSystem::List(std::string_view path,
                                          std::string_view pattern,
                                          FolderMode mode) {
  std::string normalized_path = NormalizePath(path, kFileSystemPathFlags);
  if (normalized_path.empty()) {
    return {};
  }
  path = normalized_path;
  auto [protocol_name, protocol] = GetProtocol(&path);
  if (protocol == nullptr ||
      !protocol->GetFlags().IsSet(FileProtocolFlag::kList)) {
    return {};
  }
  return protocol->List(protocol_name, path, pattern, mode,
                        {PathType::kFile, PathType::kFolder});
}

std::vector<std::string> FileSystem::ListFolders(std::string_view path,
                                                 std::string_view pattern,
                                                 FolderMode mode) {
  std::string normalized_path = NormalizePath(path, kFileSystemPathFlags);
  if (normalized_path.empty()) {
    return {};
  }
  path = normalized_path;
  auto [protocol_name, protocol] = GetProtocol(&path);
  if (protocol == nullptr ||
      !protocol->GetFlags().IsSet(FileProtocolFlag::kList)) {
    return {};
  }
  return protocol->List(protocol_name, path, pattern, mode, PathType::kFolder);
}

std::vector<std::string> FileSystem::ListFiles(std::string_view path,
                                               std::string_view pattern,
                                               FolderMode mode) {
  std::string normalized_path = NormalizePath(path, kFileSystemPathFlags);
  if (normalized_path.empty()) {
    return {};
  }
  path = normalized_path;
  auto [protocol_name, protocol] = GetProtocol(&path);
  if (protocol == nullptr ||
      !protocol->GetFlags().IsSet(FileProtocolFlag::kList)) {
    return {};
  }
  return protocol->List(protocol_name, path, pattern, mode, PathType::kFile);
}

bool FileSystem::CreateFolder(std::string_view path, FolderMode mode) {
  std::string normalized_path = NormalizePath(path, kFileSystemPathFlags);
  if (normalized_path.empty()) {
    return false;
  }
  path = normalized_path;
  auto [protocol_name, protocol] = GetProtocol(&path);
  if (protocol == nullptr ||
      !protocol->GetFlags().IsSet(FileProtocolFlag::kFolderCreate)) {
    return false;
  }
  return protocol->CreateFolder(protocol_name, path, mode);
}

bool FileSystem::DeleteFolder(std::string_view path, FolderMode mode) {
  std::string normalized_path = NormalizePath(path, kFileSystemPathFlags);
  if (normalized_path.empty()) {
    return false;
  }
  path = normalized_path;
  auto [protocol_name, protocol] = GetProtocol(&path);
  if (protocol == nullptr ||
      !protocol->GetFlags().IsSet(FileProtocolFlag::kFolderCreate)) {
    return false;
  }
  return protocol->DeleteFolder(protocol_name, path, mode);
}

bool FileSystem::DeleteFile(std::string_view path) {
  std::string normalized_path = NormalizePath(path, kFileSystemPathFlags);
  if (normalized_path.empty()) {
    return false;
  }
  path = normalized_path;
  auto [protocol_name, protocol] = GetProtocol(&path);
  if (protocol == nullptr ||
      !protocol->GetFlags().IsSet(FileProtocolFlag::kFileCreate)) {
    return false;
  }
  return protocol->DeleteFile(protocol_name, path);
}

bool FileSystem::CopyFolder(std::string_view from_path,
                            std::string_view to_path) {
  std::string normalized_from_path =
      NormalizePath(from_path, kFileSystemPathFlags);
  std::string normalized_to_path = NormalizePath(to_path, kFileSystemPathFlags);
  if (normalized_from_path.empty() || normalized_to_path.empty()) {
    return false;
  }
  from_path = normalized_from_path;
  to_path = normalized_to_path;
  auto [from_protocol_name, from_protocol] = GetProtocol(&from_path);
  if (from_protocol == nullptr) {
    return false;
  }
  auto [to_protocol_name, to_protocol] = GetProtocol(&to_path);
  if (to_protocol == nullptr) {
    return false;
  }

  auto from_flags = from_protocol->GetFlags();
  auto to_flags =
      (from_protocol == to_protocol ? from_flags : to_protocol->GetFlags());
  if (!to_flags.IsSet(FileProtocolFlag::kFolderCreate)) {
    return false;
  }

  if (from_protocol != to_protocol || from_protocol_name != to_protocol_name) {
    if (!from_flags.IsSet(
            {FileProtocolFlag::kFileRead, FileProtocolFlag::kList}) ||
        !to_flags.IsSet({FileProtocolFlag::kFolderCreate,
                         FileProtocolFlag::kFileCreate,
                         FileProtocolFlag::kFileWrite})) {
      return false;
    }
    return GenericCopyFolder(from_protocol_name, from_protocol, from_path,
                             to_protocol_name, to_protocol, to_path);
  }

  if (IsRootPath(from_path) || from_path == to_path ||
      absl::StartsWith(to_path, absl::StrCat(from_path, "/"))) {
    return false;
  }
  return from_protocol->CopyFolder(from_protocol_name, from_path, to_path);
}

bool FileSystem::CopyFile(std::string_view from_path,
                          std::string_view to_path) {
  std::string normalized_from_path =
      NormalizePath(from_path, kFileSystemPathFlags);
  std::string normalized_to_path = NormalizePath(to_path, kFileSystemPathFlags);
  if (normalized_from_path.empty() || normalized_to_path.empty()) {
    return false;
  }
  from_path = normalized_from_path;
  to_path = normalized_to_path;
  auto [from_protocol_name, from_protocol] = GetProtocol(&from_path);
  if (from_protocol == nullptr) {
    return false;
  }
  auto [to_protocol_name, to_protocol] = GetProtocol(&to_path);
  if (to_protocol == nullptr) {
    return false;
  }

  auto from_flags = from_protocol->GetFlags();
  auto to_flags =
      (from_protocol == to_protocol ? from_flags : to_protocol->GetFlags());
  if (!to_flags.IsSet(FileProtocolFlag::kFileCreate)) {
    return false;
  }
  if (from_protocol != to_protocol || from_protocol_name != to_protocol_name) {
    if (!from_flags.IsSet(FileProtocolFlag::kFileRead) ||
        !to_flags.IsSet(
            {FileProtocolFlag::kFileCreate, FileProtocolFlag::kFileWrite})) {
      return false;
    }
    return GenericCopyFile(from_protocol_name, from_protocol, from_path,
                           to_protocol_name, to_protocol, to_path);
  }
  return from_protocol->CopyFile(from_protocol_name, from_path, to_path);
}

PathInfo FileSystem::GetPathInfo(std::string_view path) {
  std::string normalized_path = NormalizePath(path, kFileSystemPathFlags);
  if (normalized_path.empty()) {
    return {};
  }
  path = normalized_path;
  auto [protocol_name, protocol] = GetProtocol(&path);
  if (protocol == nullptr ||
      !protocol->GetFlags().IsSet(FileProtocolFlag::kInfo)) {
    return {};
  }
  return protocol->GetPathInfo(protocol_name, path);
}

std::unique_ptr<File> FileSystem::OpenFile(std::string_view path,
                                           FileFlags flags) {
  std::string normalized_path = NormalizePath(path, kFileSystemPathFlags);
  if (normalized_path.empty()) {
    return nullptr;
  }
  path = normalized_path;
  auto [protocol_name, protocol] = GetProtocol(&path);
  if (protocol == nullptr) {
    return nullptr;
  }
  if (!flags.Intersects({FileFlag::kRead, FileFlag::kWrite})) {
    return nullptr;
  }
  if (flags.Intersects({FileFlag::kCreate, FileFlag::kReset}) &&
      !flags.IsSet(FileFlag::kWrite)) {
    return nullptr;
  }
  auto protocol_flags = protocol->GetFlags();
  if (flags.IsSet(FileFlag::kRead) &&
      !protocol_flags.IsSet(FileProtocolFlag::kFileRead)) {
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kWrite) &&
      !protocol_flags.IsSet(FileProtocolFlag::kFileWrite)) {
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kCreate) &&
      !protocol_flags.IsSet(FileProtocolFlag::kFileCreate)) {
    return nullptr;
  }
  auto raw_file = protocol->OpenFile(protocol_name, path, flags);
  if (raw_file == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(new File(std::move(raw_file), flags));
}

std::tuple<std::string_view, FileProtocol*> FileSystem::GetProtocol(
    std::string_view* path) {
  FileProtocol* protocol = nullptr;
  std::string_view protocol_name;
  *path = RemoveProtocol(*path, &protocol_name);
  if (protocol_name.empty()) {
    protocol = default_protocol_;
    protocol_name = default_protocol_name_;
  } else if (auto it = protocol_map_.find(protocol_name);
             it != protocol_map_.end()) {
    protocol = it->second;
  } else {
    protocol = nullptr;
  }
  return {protocol_name, protocol};
}

bool FileSystem::GenericCopyFolder(std::string_view from_protocol_name,
                                   FileProtocol* from_protocol,
                                   std::string_view from_path,
                                   std::string_view to_protocol_name,
                                   FileProtocol* to_protocol,
                                   std::string_view to_path) {
  // Ensure the folder is created.
  if (!to_protocol->CreateFolder(to_protocol_name, to_path,
                                 FolderMode::kNormal)) {
    return false;
  }

  auto from_files = from_protocol->List(from_protocol_name, from_path, {},
                                        FolderMode::kNormal, PathType::kFile);
  auto from_folders =
      from_protocol->List(from_protocol_name, from_path, {},
                          FolderMode::kNormal, PathType::kFolder);
  for (const auto& from_file : from_files) {
    std::string_view from_file_view =
        RemoveProtocol(std::string_view(from_file));
    std::string to_file = JoinPath(to_path, RemoveFolder(from_file_view));
    if (!GenericCopyFile(from_protocol_name, from_protocol, from_file_view,
                         to_protocol_name, to_protocol, to_file)) {
      return false;
    }
  }
  for (const auto& from_folder : from_folders) {
    std::string_view from_folder_view =
        RemoveProtocol(std::string_view(from_folder));
    std::string to_folder = JoinPath(to_path, RemoveFolder(from_folder_view));
    if (!GenericCopyFolder(from_protocol_name, from_protocol, from_folder_view,
                           to_protocol_name, to_protocol, to_folder)) {
      return false;
    }
  }

  return true;
}

bool FileSystem::GenericCopyFile(std::string_view from_protocol_name,
                                 FileProtocol* from_protocol,
                                 std::string_view from_path,
                                 std::string_view to_protocol_name,
                                 FileProtocol* to_protocol,
                                 std::string_view to_path) {
  auto from_file =
      from_protocol->OpenFile(from_protocol_name, from_path, FileFlag::kRead);
  if (from_file == nullptr) {
    return false;
  }
  auto to_file = to_protocol->OpenFile(
      to_protocol_name, to_path,
      {FileFlag::kCreate, FileFlag::kReset, FileFlag::kWrite});
  if (to_file == nullptr) {
    return false;
  }

  uint8_t buffer[kCopyBufferSize];
  int64_t buffer_size = 0;
  do {
    buffer_size = from_file->Read(buffer, kCopyBufferSize);
    if (buffer_size > 0) {
      if (to_file->Write(buffer, buffer_size) != buffer_size) {
        return false;
      }
    }
  } while (buffer_size == kCopyBufferSize);

  return true;
}

}  // namespace gb
