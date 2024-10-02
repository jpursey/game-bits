// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/file_protocol.h"

#include "absl/log/log.h"
#include "gb/container/queue.h"
#include "gb/file/path.h"
#include "gb/file/raw_file.h"

namespace gb {

std::vector<std::string> FileProtocol::GetDefaultNames() const { return {}; }

void FileProtocol::Lock(LockType type) {}
void FileProtocol::Unlock(LockType type) {}

PathInfo FileProtocol::GetPathInfo(std::string_view protocol_name,
                                   std::string_view path) {
  Lock(LockType::kQuery);
  auto result = DoGetPathInfo(protocol_name, path);
  Unlock(LockType::kQuery);
  return result;
}

std::vector<std::string> FileProtocol::List(std::string_view protocol_name,
                                            std::string_view path,
                                            std::string_view pattern,
                                            FolderMode mode, PathTypes types) {
  Lock(LockType::kQuery);
  auto result = DoList(protocol_name, path, pattern, mode, types);
  Unlock(LockType::kQuery);
  return result;
}

bool FileProtocol::CreateFolder(std::string_view protocol_name,
                                std::string_view path, FolderMode mode) {
  Lock(LockType::kModify);
  auto result = DoCreateFolder(protocol_name, path, mode);
  Unlock(LockType::kModify);
  return result;
}

bool FileProtocol::CopyFolder(std::string_view protocol_name,
                              std::string_view from_path,
                              std::string_view to_path) {
  Lock(LockType::kModify);
  auto result = DoCopyFolder(protocol_name, from_path, to_path);
  Unlock(LockType::kModify);
  return result;
}

bool FileProtocol::DeleteFolder(std::string_view protocol_name,
                                std::string_view path, FolderMode mode) {
  Lock(LockType::kModify);
  auto result = DoDeleteFolder(protocol_name, path, mode);
  Unlock(LockType::kModify);
  return result;
}

bool FileProtocol::CopyFile(std::string_view protocol_name,
                            std::string_view from_path,
                            std::string_view to_path) {
  Lock(LockType::kModify);
  auto result = DoCopyFile(protocol_name, from_path, to_path);
  Unlock(LockType::kModify);
  return result;
}

bool FileProtocol::DeleteFile(std::string_view protocol_name,
                              std::string_view path) {
  Lock(LockType::kModify);
  auto result = DoDeleteFile(protocol_name, path);
  Unlock(LockType::kModify);
  return result;
}

std::unique_ptr<RawFile> FileProtocol::OpenFile(std::string_view protocol_name,
                                                std::string_view path,
                                                FileFlags flags) {
  LockType lock_type = LockType::kOpenRead;
  if (flags.IsSet(FileFlag::kCreate)) {
    lock_type = LockType::kModify;
  } else if (flags.IsSet(FileFlag::kWrite)) {
    lock_type = LockType::kOpenWrite;
  }
  Lock(lock_type);
  auto result = DoOpenFile(protocol_name, path, flags);
  Unlock(lock_type);
  return result;
}

PathInfo FileProtocol::DoGetPathInfo(std::string_view protocol_name,
                                     std::string_view path) {
  LOG(ERROR) << "FileProtocol::DoGetPathInfo not implemented.";
  return {};
}

std::vector<std::string> FileProtocol::DoList(std::string_view protocol_name,
                                              std::string_view path,
                                              std::string_view pattern,
                                              FolderMode mode,
                                              PathTypes types) {
  if (DoGetPathInfo(protocol_name, path).type != PathType::kFolder) {
    return {};
  }
  std::vector<std::string> result;
  std::vector<std::string> paths = BasicList(protocol_name, path);
  Queue<std::string> remaining(
      std::max<int>(static_cast<int>(paths.size()), 100));
  for (auto& path : paths) {
    remaining.emplace(std::move(path));
  }
  while (!remaining.empty()) {
    std::string current = std::move(remaining.front());
    std::string_view current_path = RemoveProtocol(std::string_view(current));
    remaining.pop();

    const PathInfo current_info =
        DoGetPathInfo(protocol_name, RemoveProtocol(current_path));
    if (current_info.type == PathType::kFolder &&
        mode == FolderMode::kRecursive) {
      paths = BasicList(protocol_name, current_path);
      for (auto& path : paths) {
        remaining.emplace(std::move(path));
      }
    }

    if (types != kAllPathTypes && !types.IsSet(current_info.type)) {
      continue;
    }
    if (!pattern.empty() &&
        !PathMatchesPattern(RemoveFolder(current_path), pattern)) {
      continue;
    }

    result.emplace_back(std::move(current));
  }
  return result;
}

bool FileProtocol::DoCreateFolder(std::string_view protocol_name,
                                  std::string_view path, FolderMode mode) {
  PathInfo info = DoGetPathInfo(protocol_name, path);
  if (info.type != PathType::kInvalid) {
    return info.type == PathType::kFolder;
  }

  if (mode == FolderMode::kNormal) {
    info = DoGetPathInfo(protocol_name, RemoveFilename(path));
    if (info.type != PathType::kFolder) {
      return false;
    }
    return BasicCreateFolder(protocol_name, path);
  }

  std::vector<std::string_view> paths;
  do {
    paths.emplace_back(path);
    path = RemoveFilename(path);
    info = DoGetPathInfo(protocol_name, path);
  } while (info.type == PathType::kInvalid);
  if (info.type != PathType::kFolder) {
    return false;
  }
  for (auto it = paths.crbegin(); it != paths.crend(); ++it) {
    if (!BasicCreateFolder(protocol_name, *it)) {
      return false;
    }
  }
  return true;
}

bool FileProtocol::DoDeleteFolder(std::string_view protocol_name,
                                  std::string_view path, FolderMode mode) {
  auto info = DoGetPathInfo(protocol_name, path);
  if (info.type != PathType::kFolder) {
    return info.type == PathType::kInvalid;
  }
  if (IsRootPath(path)) {
    return false;
  }

  auto subfolders =
      DoList(protocol_name, path, {}, FolderMode::kNormal, PathType::kFolder);
  auto files =
      DoList(protocol_name, path, {}, FolderMode::kNormal, PathType::kFile);
  if (mode == FolderMode::kNormal && (!subfolders.empty() || !files.empty())) {
    return false;
  }

  for (const auto& subfolder : subfolders) {
    if (!DoDeleteFolder(protocol_name, RemoveProtocol(subfolder), mode)) {
      return false;
    }
  }
  for (const auto& file : files) {
    if (!DoDeleteFile(protocol_name, RemoveProtocol(file))) {
      return false;
    }
  }

  return BasicDeleteFolder(protocol_name, path);
}

bool FileProtocol::DoCopyFolder(std::string_view protocol_name,
                                std::string_view from_path,
                                std::string_view to_path) {
  auto to_info = DoGetPathInfo(protocol_name, to_path);
  if (to_info.type != PathType::kInvalid && to_info.type != PathType::kFolder) {
    return false;
  }
  auto from_info = DoGetPathInfo(protocol_name, from_path);
  if (from_info.type != PathType::kFolder) {
    return false;
  }

  auto from_files = DoList(protocol_name, from_path, {}, FolderMode::kNormal,
                           PathType::kFile);
  auto from_folders = DoList(protocol_name, from_path, {}, FolderMode::kNormal,
                             PathType::kFolder);

  if (to_info.type == PathType::kInvalid) {
    if (!DoCreateFolder(protocol_name, to_path, FolderMode::kNormal)) {
      return false;
    }
  }

  for (const auto& from_file : from_files) {
    from_path = RemoveProtocol(std::string_view{from_file});
    std::string to_file = JoinPath(to_path, RemoveFolder(from_path));
    if (!DoCopyFile(protocol_name, from_path, to_file)) {
      return false;
    }
  }
  for (const auto& from_folder : from_folders) {
    from_path = RemoveProtocol(std::string_view{from_folder});
    std::string to_folder = JoinPath(to_path, RemoveFolder(from_path));
    if (!DoCopyFolder(protocol_name, from_path, to_folder)) {
      return false;
    }
  }

  return true;
}

bool FileProtocol::DoCopyFile(std::string_view protocol_name,
                              std::string_view from_path,
                              std::string_view to_path) {
  if (DoGetPathInfo(protocol_name, from_path).type != PathType::kFile) {
    return false;
  }
  std::string new_to_path;
  auto to_info = DoGetPathInfo(protocol_name, to_path);
  if (to_info.type == PathType::kFolder) {
    return false;
  } else if (to_info.type == PathType::kInvalid &&
             DoGetPathInfo(protocol_name, RemoveFilename(to_path)).type !=
                 PathType::kFolder) {
    return false;
  } else if (from_path == to_path) {
    return true;
  }
  return BasicCopyFile(protocol_name, from_path, to_path);
}

bool FileProtocol::DoDeleteFile(std::string_view protocol_name,
                                std::string_view path) {
  auto info = DoGetPathInfo(protocol_name, path);
  if (info.type != PathType::kFile) {
    return info.type == PathType::kInvalid;
  }
  return BasicDeleteFile(protocol_name, path);
}

std::unique_ptr<RawFile> FileProtocol::DoOpenFile(
    std::string_view protocol_name, std::string_view path, FileFlags flags) {
  auto info = DoGetPathInfo(protocol_name, path);
  if (info.type == PathType::kFolder) {
    return nullptr;
  } else if (info.type == PathType::kInvalid) {
    if (!flags.IsSet(FileFlag::kCreate)) {
      return nullptr;
    }
    if (DoGetPathInfo(protocol_name, RemoveFilename(path)).type !=
        PathType::kFolder) {
      return nullptr;
    }
  } else if (flags.IsSet(FileFlag::kCreate)) {
    flags -= FileFlag::kCreate;
  }
  return BasicOpenFile(protocol_name, path, flags);
}

std::vector<std::string> FileProtocol::BasicList(std::string_view protocol_name,
                                                 std::string_view path) {
  LOG(ERROR) << "FileProtocol::BasicList not implemented.";
  return {};
}

bool FileProtocol::BasicCreateFolder(std::string_view protocol_name,
                                     std::string_view path) {
  LOG(ERROR) << "FileProtocol::BasicCreateFolder not implemented.";
  return false;
}

bool FileProtocol::BasicDeleteFolder(std::string_view protocol_name,
                                     std::string_view path) {
  LOG(ERROR) << "FileProtocol::BasicDeleteFolder not implemented.";
  return false;
}

bool FileProtocol::BasicCopyFile(std::string_view protocol_name,
                                 std::string_view from_path,
                                 std::string_view to_path) {
  auto from_file = DoOpenFile(protocol_name, from_path, FileFlag::kRead);
  if (from_file == nullptr) {
    return false;
  }
  auto to_file =
      DoOpenFile(protocol_name, to_path,
                 {FileFlag::kCreate, FileFlag::kReset, FileFlag::kWrite});
  if (to_file == nullptr) {
    return false;
  }

  uint8_t buffer[kBasicCopyBufferSize];
  int64_t buffer_size = 0;
  do {
    buffer_size = from_file->Read(buffer, kBasicCopyBufferSize);
    if (buffer_size > 0) {
      if (to_file->Write(buffer, buffer_size) != buffer_size) {
        return false;
      }
    }
  } while (buffer_size == kBasicCopyBufferSize);

  return true;
}

bool FileProtocol::BasicDeleteFile(std::string_view protocol_name,
                                   std::string_view path) {
  LOG(ERROR) << "FileProtocol::BasicDeleteFile not implemented.";
  return false;
}

std::unique_ptr<RawFile> FileProtocol::BasicOpenFile(
    std::string_view protocol_name, std::string_view path, FileFlags flags) {
  LOG(ERROR) << "FileProtocol::BasicOpenFile not implemented.";
  return nullptr;
}

}  // namespace gb
