#include "gbits/file/file_protocol.h"

#include <deque>

#include "gbits/file/path.h"
#include "gbits/file/raw_file.h"
#include "glog/logging.h"

namespace gb {

std::vector<std::string> FileProtocol::GetDefaultNames() const { return {}; }

PathInfo FileProtocol::GetPathInfo(std::string_view protocol_name,
                                   std::string_view path) {
  LOG(ERROR) << "FileProtocol::GetPathInfo not implemented.";
  return {};
}

std::vector<std::string> FileProtocol::List(std::string_view protocol_name,
                                            std::string_view path,
                                            std::string_view pattern,
                                            FolderMode mode, PathTypes types) {
  if (GetPathInfo(protocol_name, path).type != PathType::kFolder) {
    return {};
  }
  std::vector<std::string> result;
  std::vector<std::string> paths = BasicList(protocol_name, path);
  std::deque<std::string> remaining(paths.begin(), paths.end());
  while (!remaining.empty()) {
    std::string current = std::move(remaining.front());
    std::string_view current_path = RemoveProtocol(std::string_view(current));
    remaining.pop_front();

    const PathInfo current_info =
        GetPathInfo(protocol_name, RemoveProtocol(current_path));
    if (current_info.type == PathType::kFolder &&
        mode == FolderMode::kRecursive) {
      paths = BasicList(protocol_name, current_path);
      remaining.insert(remaining.end(), paths.begin(), paths.end());
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

bool FileProtocol::CreateFolder(std::string_view protocol_name,
                                std::string_view path, FolderMode mode) {
  PathInfo info = GetPathInfo(protocol_name, path);
  if (info.type != PathType::kInvalid) {
    return info.type == PathType::kFolder;
  }

  if (mode == FolderMode::kNormal) {
    info = GetPathInfo(protocol_name, RemoveFilename(path));
    if (info.type != PathType::kFolder) {
      return false;
    }
    return BasicCreateFolder(protocol_name, path);
  }

  std::vector<std::string_view> paths;
  do {
    paths.emplace_back(path);
    path = RemoveFilename(path);
    info = GetPathInfo(protocol_name, path);
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

bool FileProtocol::DeleteFolder(std::string_view protocol_name,
                                std::string_view path, FolderMode mode) {
  auto info = GetPathInfo(protocol_name, path);
  if (info.type != PathType::kFolder) {
    return info.type == PathType::kInvalid;
  }
  if (IsRootPath(path)) {
    return false;
  }

  auto subfolders =
      List(protocol_name, path, {}, FolderMode::kNormal, PathType::kFolder);
  auto files =
      List(protocol_name, path, {}, FolderMode::kNormal, PathType::kFile);
  if (mode == FolderMode::kNormal && (!subfolders.empty() || !files.empty())) {
    return false;
  }

  for (const auto& subfolder : subfolders) {
    if (!DeleteFolder(protocol_name, RemoveProtocol(subfolder), mode)) {
      return false;
    }
  }
  for (const auto& file : files) {
    if (!DeleteFile(protocol_name, RemoveProtocol(file))) {
      return false;
    }
  }

  return BasicDeleteFolder(protocol_name, path);
}

bool FileProtocol::CopyFolder(std::string_view protocol_name,
                              std::string_view from_path,
                              std::string_view to_path) {
  auto to_info = GetPathInfo(protocol_name, to_path);
  if (to_info.type != PathType::kInvalid && to_info.type != PathType::kFolder) {
    return false;
  }
  auto from_info = GetPathInfo(protocol_name, from_path);
  if (from_info.type != PathType::kFolder) {
    return false;
  }

  auto from_files =
      List(protocol_name, from_path, {}, FolderMode::kNormal, PathType::kFile);
  auto from_folders = List(protocol_name, from_path, {}, FolderMode::kNormal,
                           PathType::kFolder);

  if (to_info.type == PathType::kInvalid) {
    if (!CreateFolder(protocol_name, to_path, FolderMode::kNormal)) {
      return false;
    }
  }

  for (const auto& from_file : from_files) {
    from_path = RemoveProtocol(std::string_view{from_file});
    std::string to_file = JoinPath(to_path, RemoveFolder(from_path));
    if (!CopyFile(protocol_name, from_path, to_file)) {
      return false;
    }
  }
  for (const auto& from_folder : from_folders) {
    from_path = RemoveProtocol(std::string_view{from_folder});
    std::string to_folder = JoinPath(to_path, RemoveFolder(from_path));
    if (!CopyFolder(protocol_name, from_path, to_folder)) {
      return false;
    }
  }

  return true;
}

bool FileProtocol::CopyFile(std::string_view protocol_name,
                            std::string_view from_path,
                            std::string_view to_path) {
  if (GetPathInfo(protocol_name, from_path).type != PathType::kFile) {
    return false;
  }
  std::string new_to_path;
  auto to_info = GetPathInfo(protocol_name, to_path);
  if (to_info.type == PathType::kFolder) {
    return false;
  } else if (to_info.type == PathType::kInvalid &&
             GetPathInfo(protocol_name, RemoveFilename(to_path)).type !=
                 PathType::kFolder) {
    return false;
  } else if (from_path == to_path) {
    return true;
  }
  return BasicCopyFile(protocol_name, from_path, to_path);
}

bool FileProtocol::DeleteFile(std::string_view protocol_name,
                              std::string_view path) {
  auto info = GetPathInfo(protocol_name, path);
  if (info.type != PathType::kFile) {
    return info.type == PathType::kInvalid;
  }
  return BasicDeleteFile(protocol_name, path);
}

std::unique_ptr<RawFile> FileProtocol::OpenFile(std::string_view protocol_name,
                                                std::string_view path,
                                                FileFlags flags) {
  auto info = GetPathInfo(protocol_name, path);
  if (info.type == PathType::kFolder) {
    return nullptr;
  } else if (info.type == PathType::kInvalid) {
    if (!flags.IsSet(FileFlag::kCreate)) {
      return nullptr;
    }
    if (GetPathInfo(protocol_name, RemoveFilename(path)).type !=
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
  auto from_file = OpenFile(protocol_name, from_path, FileFlag::kRead);
  if (from_file == nullptr) {
    return false;
  }
  auto to_file =
      OpenFile(protocol_name, to_path,
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
