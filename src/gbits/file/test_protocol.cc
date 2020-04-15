#include "gbits/file/test_protocol.h"

#include <cstring>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "gbits/file/path.h"
#include "gbits/file/raw_file.h"
#include "glog/logging.h"

namespace gb {

namespace {

class TestFile : public RawFile {
 public:
  TestFile(TestProtocol::FileState* state, FileFlags flags) : state_(state) {
    CHECK_EQ(state_->file, nullptr) << "TestProtocol does not support multiple "
                                       "simultaneous files open on the same "
                                       "path.";
    state_->file = this;
    state_->flags = flags;
    state_->ResetCounts();
  }
  ~TestFile() override {
    state_->file = nullptr;
    state_->position = 0;
  }

  int64_t SeekEnd() override {
    state_->seek_end_count += 1;
    if (state_->position < 0 || state_->fail_seek) {
      return -1;
    }
    state_->position = GetSize();
    return state_->position;
  }

  int64_t SeekTo(int64_t position) override {
    state_->seek_to_count += 1;
    if (state_->position < 0 || state_->fail_seek) {
      state_->position = -1;
      return -1;
    }
    state_->position = std::clamp(position, 0LL, GetSize());
    return state_->position;
  }

  int64_t Write(const void* buffer, int64_t size) override {
    state_->write_count += 1;
    state_->request_bytes_written += size;
    if (!state_->flags.IsSet(FileFlag::kWrite)) {
      state_->invalid_call_count += 1;
      return 0;
    }
    if (state_->position < 0) {
      return 0;
    }
    const int64_t write_position = state_->position;
    if (state_->fail_write_after >= 0 &&
        state_->request_bytes_written > state_->fail_write_after) {
      state_->position = -1;
      if (state_->request_bytes_written - size < state_->fail_write_after) {
        size -= state_->request_bytes_written - state_->fail_write_after;
      } else {
        return 0;
      }
    }
    if (write_position + size > GetSize()) {
      state_->contents.resize(write_position + size);
    }
    state_->bytes_written += size;
    std::memcpy(&state_->contents[write_position], buffer, size);
    if (state_->position >= 0) {
      state_->position += size;
    }
    return size;
  }

  int64_t Read(void* buffer, int64_t size) override {
    state_->read_count += 1;
    state_->request_bytes_read += size;
    if (!state_->flags.IsSet(FileFlag::kRead)) {
      state_->invalid_call_count += 1;
      return 0;
    }
    if (state_->position < 0) {
      return 0;
    }
    const int64_t read_position = state_->position;
    if (state_->fail_read_after >= 0 &&
        state_->request_bytes_read > state_->fail_read_after) {
      state_->position = -1;
      if (state_->request_bytes_read - size < state_->fail_read_after) {
        size -= state_->request_bytes_read - state_->fail_read_after;
      } else {
        return 0;
      }
    }
    if (read_position + size > GetSize()) {
      size = GetSize() - read_position;
    }
    state_->bytes_read += size;
    std::memcpy(buffer, &state_->contents[read_position], size);
    if (state_->position >= 0) {
      state_->position += size;
    }
    return size;
  }

 private:
  int64_t GetSize() const {
    return static_cast<int64_t>(state_->contents.size());
  }

  TestProtocol::FileState* const state_;
};

}  // namespace

void TestProtocol::FileState::ResetCounts() {
  invalid_call_count = 0;
  seek_end_count = 0;
  seek_to_count = 0;
  write_count = 0;
  read_count = 0;
  request_bytes_written = 0;
  bytes_written = 0;
  request_bytes_read = 0;
  bytes_read = 0;
}

void TestProtocol::State::ResetCounts() {
  invalid_call_count = 0;
  list_count = 0;
  create_folder_count = 0;
  delete_folder_count = 0;
  delete_file_count = 0;
  copy_folder_count = 0;
  copy_file_count = 0;
  open_file_count = 0;
  basic_list_count = 0;
  basic_create_folder_count = 0;
  basic_delete_folder_count = 0;
  basic_delete_file_count = 0;
  basic_copy_file_count = 0;
  basic_open_file_count = 0;
  for (auto& path_state : paths) {
    auto file_state = path_state.second.GetFile();
    if (file_state != nullptr) {
      file_state->ResetCounts();
    }
  }
}

void TestProtocol::State::ResetState() {
  paths.clear();
  ResetCounts();
}

FileProtocolFlags TestProtocol::GetFlags() const { return state_->flags; }

std::vector<std::string> TestProtocol::GetDefaultNames() const {
  return state_->default_names;
}

PathInfo TestProtocol::GetPathInfo(std::string_view protocol_name,
                                   std::string_view path) {
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path) ||
      !state_->flags.IsSet(FileProtocolFlag::kInfo)) {
    state_->invalid_call_count += 1;
    return {};
  }
  if (path == "/") {
    return {PathType::kFolder};
  }
  auto& path_state = state_->paths[absl::StrCat(path)];
  return {path_state.GetType(), path_state.GetSize()};
}

std::vector<std::string> TestProtocol::List(std::string_view protocol_name,
                                            std::string_view path,
                                            std::string_view pattern,
                                            FolderMode mode, PathTypes types) {
  state_->list_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path) ||
      !state_->flags.IsSet(FileProtocolFlag::kList)) {
    state_->invalid_call_count += 1;
    return {};
  }
  return FileProtocol::List(protocol_name, path, pattern, mode, types);
}

bool TestProtocol::CreateFolder(std::string_view protocol_name,
                                std::string_view path, FolderMode mode) {
  state_->create_folder_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path) ||
      !state_->flags.IsSet(FileProtocolFlag::kFolderCreate)) {
    state_->invalid_call_count += 1;
    return false;
  }
  return FileProtocol::CreateFolder(protocol_name, path, mode);
}

bool TestProtocol::CopyFolder(std::string_view protocol_name,
                              std::string_view from_path,
                              std::string_view to_path) {
  state_->copy_folder_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(from_path) ||
      !IsValidPath(to_path) ||
      !state_->flags.IsSet(FileProtocolFlag::kFolderCreate)) {
    state_->invalid_call_count += 1;
    return false;
  }
  return FileProtocol::CopyFolder(protocol_name, from_path, to_path);
}

bool TestProtocol::DeleteFolder(std::string_view protocol_name,
                                std::string_view path, FolderMode mode) {
  state_->delete_folder_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path) ||
      !state_->flags.IsSet(FileProtocolFlag::kFolderCreate)) {
    state_->invalid_call_count += 1;
    return false;
  }
  return FileProtocol::DeleteFolder(protocol_name, path, mode);
}

bool TestProtocol::CopyFile(std::string_view protocol_name,
                            std::string_view from_path,
                            std::string_view to_path) {
  state_->copy_file_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(from_path) ||
      !IsValidPath(to_path) ||
      !state_->flags.IsSet(FileProtocolFlag::kFileCreate)) {
    state_->invalid_call_count += 1;
    return false;
  }
  return FileProtocol::CopyFile(protocol_name, from_path, to_path);
}

bool TestProtocol::DeleteFile(std::string_view protocol_name,
                              std::string_view path) {
  state_->delete_file_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path) ||
      !state_->flags.IsSet(FileProtocolFlag::kFileCreate)) {
    state_->invalid_call_count += 1;
    return false;
  }
  return FileProtocol::DeleteFile(protocol_name, path);
}

std::unique_ptr<RawFile> TestProtocol::OpenFile(std::string_view protocol_name,
                                                std::string_view path,
                                                FileFlags flags) {
  state_->open_file_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path)) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kCreate) &&
      !state_->flags.IsSet(FileProtocolFlag::kFileCreate)) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kReset) && !flags.IsSet(FileFlag::kWrite)) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kWrite) &&
      !state_->flags.IsSet(FileProtocolFlag::kFileWrite)) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kRead) &&
      !state_->flags.IsSet(FileProtocolFlag::kFileRead)) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  return FileProtocol::OpenFile(protocol_name, path, flags);
}

std::vector<std::string> TestProtocol::BasicList(std::string_view protocol_name,
                                                 std::string_view path) {
  state_->basic_list_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path)) {
    state_->invalid_call_count += 1;
    return {};
  }
  if (!state_->flags.IsSet(FileProtocolFlag::kList)) {
    state_->invalid_call_count += 1;
    return {};
  }
  if (path != "/" &&
      state_->paths[absl::StrCat(path)].GetType() != PathType::kFolder) {
    state_->invalid_call_count += 1;
    return {};
  }
  std::string prefix;
  if (path == "/") {
    prefix = "/";
  } else {
    prefix = absl::StrCat(path, "/");
  }
  std::vector<std::string> paths;
  for (auto it = state_->paths.lower_bound(prefix);
       it != state_->paths.end() && absl::StartsWith(it->first, prefix); ++it) {
    if (it->second.GetType() == PathType::kInvalid) {
      continue;
    }
    std::string_view item_path = it->first;
    item_path.remove_prefix(prefix.size());
    if (item_path.find('/') != std::string_view::npos) {
      continue;
    }
    paths.emplace_back(absl::StrCat(protocol_name, ":", prefix, item_path));
  }
  return paths;
}

bool TestProtocol::BasicCreateFolder(std::string_view protocol_name,
                                     std::string_view path) {
  state_->basic_create_folder_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path) ||
      path == "/") {
    state_->invalid_call_count += 1;
    return false;
  }
  if (!state_->flags.IsSet(FileProtocolFlag::kFolderCreate)) {
    state_->invalid_call_count += 1;
    return false;
  }
  std::string_view parent_path = RemoveFilename(path);
  if (parent_path != "/") {
    auto& parent_state = state_->paths[absl::StrCat(parent_path)];
    if (parent_state.GetType() != PathType::kFolder) {
      state_->invalid_call_count += 1;
      return false;
    }
  }
  auto& path_state = state_->paths[absl::StrCat(path)];
  if (path_state.GetType() != PathType::kInvalid) {
    state_->invalid_call_count += 1;
    return false;
  }
  if (path == state_->fail_path) {
    return false;
  }
  path_state = PathState::NewFolder();
  return true;
}

bool TestProtocol::BasicDeleteFolder(std::string_view protocol_name,
                                     std::string_view path) {
  state_->basic_delete_folder_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path)) {
    state_->invalid_call_count += 1;
    return false;
  }
  if (!state_->flags.IsSet(FileProtocolFlag::kFolderCreate)) {
    state_->invalid_call_count += 1;
    return false;
  }
  auto& path_state = state_->paths[absl::StrCat(path)];
  if (path_state.GetType() != PathType::kFolder) {
    state_->invalid_call_count += 1;
    return false;
  }
  auto child_paths = BasicList(protocol_name, path);
  state_->basic_list_count -= 1;  // Don't count internal use of BasicList call.
  if (!child_paths.empty()) {
    state_->invalid_call_count += 1;
    return false;
  }
  if (path == state_->fail_path) {
    return false;
  }
  path_state = {};
  return true;
}

bool TestProtocol::BasicCopyFile(std::string_view protocol_name,
                                 std::string_view from_path,
                                 std::string_view to_path) {
  state_->basic_copy_file_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(from_path) ||
      !IsValidPath(to_path)) {
    state_->invalid_call_count += 1;
    return false;
  }
  if (!state_->flags.IsSet(FileProtocolFlag::kFileCreate)) {
    state_->invalid_call_count += 1;
    return false;
  }
  auto& from_state = state_->paths[absl::StrCat(from_path)];
  if (from_state.GetType() != PathType::kFile) {
    state_->invalid_call_count += 1;
    return false;
  }
  auto& to_state = state_->paths[absl::StrCat(to_path)];
  if (to_state.GetType() == PathType::kFolder) {
    state_->invalid_call_count += 1;
    return false;
  }
  if (from_path == state_->fail_path || to_path == state_->fail_path) {
    return false;
  }
  if (state_->implement_copy) {
    to_state = PathState::NewFile(from_state.GetFile()->contents);
    return true;
  }
  return FileProtocol::BasicCopyFile(protocol_name, from_path, to_path);
}

bool TestProtocol::BasicDeleteFile(std::string_view protocol_name,
                                   std::string_view path) {
  state_->basic_delete_file_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path)) {
    state_->invalid_call_count += 1;
    return false;
  }
  if (!state_->flags.IsSet(FileProtocolFlag::kFileCreate)) {
    state_->invalid_call_count += 1;
    return false;
  }
  auto& path_state = state_->paths[absl::StrCat(path)];
  if (path_state.GetType() != PathType::kFile) {
    state_->invalid_call_count += 1;
    return false;
  }
  if (path == state_->fail_path) {
    return false;
  }
  path_state = {};
  return true;
}

std::unique_ptr<RawFile> TestProtocol::BasicOpenFile(
    std::string_view protocol_name, std::string_view path, FileFlags flags) {
  state_->basic_open_file_count += 1;
  if (!IsValidProtocolName(protocol_name) || !IsValidPath(path)) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kCreate) &&
      !state_->flags.IsSet(FileProtocolFlag::kFileCreate)) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kReset) && !flags.IsSet(FileFlag::kWrite)) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kWrite) &&
      !state_->flags.IsSet(FileProtocolFlag::kFileWrite)) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kRead) &&
      !state_->flags.IsSet(FileProtocolFlag::kFileRead)) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  auto& path_state = state_->paths[absl::StrCat(path)];
  if (path_state.GetType() == PathType::kFolder) {
    state_->invalid_call_count += 1;
    return nullptr;
  }
  if (path_state.GetType() == PathType::kFile) {
    if (flags.IsSet(FileFlag::kCreate)) {
      state_->invalid_call_count += 1;
      return nullptr;
    }
    if (flags.IsSet(FileFlag::kReset)) {
      path_state.GetFile()->contents.clear();
    }
  } else {  // path_state.GetType() == PathType::kInvalid
    if (!flags.IsSet(FileFlag::kCreate)) {
      state_->invalid_call_count += 1;
      return nullptr;
    }
    std::string_view parent_path = RemoveFilename(path);
    if (parent_path != "/") {
      auto& parent_state = state_->paths[absl::StrCat(parent_path)];
      if (parent_state.GetType() != PathType::kFolder) {
        state_->invalid_call_count += 1;
        return nullptr;
      }
    }
    if (path != state_->fail_path && path != state_->open_fail_path) {
      path_state = PathState::NewFile();
      if (path == state_->io_fail_path) {
        path_state.GetFile()->position = -1;
      }
    }
  }
  if (path == state_->fail_path || path == state_->open_fail_path) {
    return nullptr;
  }
  auto raw_file = std::make_unique<TestFile>(path_state.GetFile(), flags);
  if (path == state_->io_fail_path) {
    path_state.GetFile()->position = -1;
  }
  return raw_file;
}

}  // namespace gb
