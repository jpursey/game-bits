#include "gb/file/local_file_protocol.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "gb/base/context_builder.h"
#include "gb/file/path.h"
#include "gb/file/raw_file.h"
#include "glog/logging.h"

namespace gb {

namespace {

namespace fs = std::filesystem;

class LocalFile : public RawFile {
 public:
  explicit LocalFile(std::fstream file) : file_(std::move(file)) {}
  ~LocalFile() override {}

  int64_t SeekEnd() override;
  int64_t SeekTo(int64_t position) override;
  int64_t Write(const void* buffer, int64_t size) override;
  int64_t Read(void* buffer, int64_t size) override;

 private:
  int64_t GetPosition();

  std::fstream file_;
  int64_t position_ = 0;
};

inline int64_t LocalFile::GetPosition() {
  auto pos = file_.tellg();
  if (pos == std::fstream::pos_type(-1)) {
    position_ = -1;
  } else {
    position_ = static_cast<int64_t>(pos);
  }
  return position_;
}

int64_t LocalFile::SeekEnd() {
  file_.seekg(0, std::fstream::end);
  return GetPosition();
}

int64_t LocalFile::SeekTo(int64_t position) {
  file_.seekg(static_cast<std::fstream::pos_type>(position));
  return GetPosition();
}

int64_t LocalFile::Write(const void* buffer, int64_t size) {
  file_.write(reinterpret_cast<const char*>(buffer),
              static_cast<std::streamsize>(size));
  if (file_.fail()) {
    int64_t position = position_;
    if (GetPosition() < 0) {
      return 0;
    }
    size = std::max<int64_t>(position_ - position, 0);
  }
  position_ += size;
  return size;
}

int64_t LocalFile::Read(void* buffer, int64_t size) {
  file_.read(reinterpret_cast<char*>(buffer),
             static_cast<std::streamsize>(size));
  if (file_.eof()) {
    size = file_.gcount();
    file_.clear();
    SeekEnd();
    return size;
  }
  if (file_.fail()) {
    int64_t position = position_;
    if (GetPosition() < 0) {
      size = 0;
    } else {
      size = std::max<int64_t>(position_ - position, 0);
    }
  }
  position_ += size;
  return size;
}

std::string ToString(const fs::path& path) {
  return NormalizePath(path.generic_u8string());
}

fs::path ToPath(std::string_view path) {
  return fs::path(path, fs::path::generic_format);
}

}  // namespace

std::unique_ptr<LocalFileProtocol> LocalFileProtocol::Create(
    Contract contract) {
  ValidatedContext context = std::move(contract);
  if (!context.IsValid()) {
    return nullptr;
  }

  // Used throughout to avoid exceptions.
  std::error_code error;

  // Change relative paths to be relative to the current working directory for
  // this process.
  std::string root = context.GetValue<std::string>(kKeyRoot);
  if (!IsPathAbsolute(root)) {
    auto current_path = fs::current_path(error);
    if (error) {
      LOG(ERROR) << "Cannot access current directory when resolving requested "
                    "relative root path \""
                 << root << "\". Error: " << error.message();
      return nullptr;
    }
    root = NormalizePath(JoinPath(ToString(current_path), root));
    if (root.empty()) {
      LOG(ERROR) << "Requested root path \""
                 << context.GetValue<std::string>(kKeyRoot)
                 << "\" could not be resolved against current directory \""
                 << current_path << "\"";
      return nullptr;
    }
  }

  // Determine the root directory and requested sub directory to make into the
  // root, if there is one.
  std::string sub_directory;
  auto root_status = fs::status(ToPath(root), error);
  if (!fs::exists(root_status)) {
    root = RemoveFilename(root, &sub_directory);
    auto parent_status = fs::status(ToPath(root), error);
    if (!fs::status_known(parent_status)) {
      LOG(ERROR) << "Could not access parent path of root path \""
                 << context.GetValue<std::string>(kKeyRoot)
                 << "\". Error: " << error.message();
      return nullptr;
    }
    if (!fs::is_directory(parent_status)) {
      LOG(ERROR) << "Requested root path \""
                 << context.GetValue<std::string>(kKeyRoot)
                 << "\" is not relative to a valid directory.";
      return nullptr;
    }
  } else if (!fs::status_known(root_status)) {
    LOG(ERROR) << "Error accessing requested root path \""
               << context.GetValue<std::string>(kKeyRoot)
               << "\". Error: " << error.message();
    return nullptr;
  } else if (!fs::is_directory(root_status)) {
    LOG(ERROR) << "Requested root path \""
               << context.GetValue<std::string>(kKeyRoot)
               << "\" exists but is not a valid directory.";
    return nullptr;
  }

  // Determine a new unique root if requested.
  if (context.GetValue<bool>(kKeyUniqueRoot)) {
    std::random_device seed_device;
    std::mt19937 generator(seed_device());
    std::uniform_int_distribution<> random(1, 999999);
    if (!sub_directory.empty()) {
      sub_directory += "_";
    }
    int attempts = 0;
    do {
      std::string new_sub_directory = absl::StrCat(
          sub_directory, absl::Dec(random(generator), absl::kZeroPad6));
      std::string path = JoinPath(root, new_sub_directory);
      auto path_status = fs::status(ToPath(path), error);
      if (!fs::exists(path_status)) {
        sub_directory = new_sub_directory;
        break;
      }
      if (!fs::status_known(path_status)) {
        LOG(ERROR) << "Could not find unique root path under \"" << root
                   << "\" due to error when accessing generated path \"" << path
                   << "\". Error: " << error.message();
        return nullptr;
      }
    } while (++attempts < 100);
    if (attempts >= 100) {
      LOG(ERROR) << "Could not find unique root path under \"" << root
                 << "\" with prefix \"" << sub_directory
                 << "\" after 100 attempts.";
      return nullptr;
    }
  }

  // If a new root path is requested, attempt to create it now.
  if (!sub_directory.empty()) {
    root = JoinPath(root, sub_directory);
    if (!fs::create_directory(ToPath(root), error)) {
      LOG(ERROR) << "Failed to create root directory \"" << root
                 << "\". Error: " << error.message();
      return nullptr;
    }
  }

  return absl::WrapUnique(new LocalFileProtocol(root, context));
}

std::unique_ptr<LocalFileProtocol> LocalFileProtocol::CreateTemp(
    std::string_view temp_prefix) {
  std::error_code error;
  auto temp_path = fs::temp_directory_path(error);
  if (error) {
    LOG(ERROR) << "Failed to retrieve temp directory. Error: "
               << error.message();
    return nullptr;
  }

  return Create(ContextBuilder()
                    .SetValue<std::string>(
                        kKeyRoot, JoinPath(ToString(temp_path), temp_prefix))
                    .SetValue<bool>(kKeyUniqueRoot, true)
                    .SetValue<bool>(kKeyDeleteAtExit, true)
                    .Build());
}

LocalFileProtocol::LocalFileProtocol(std::string_view root,
                                     const ValidatedContext& context)
    : flags_(context.GetValue<FileProtocolFlags>()),
      root_(root.data(), root.size()),
      unique_root_(context.GetValue<bool>(kKeyUniqueRoot)),
      delete_at_exit_(context.GetValue<bool>(kKeyDeleteAtExit)) {}

LocalFileProtocol::~LocalFileProtocol() {
  if (!delete_at_exit_) {
    return;
  }

  std::error_code error;

  if (unique_root_) {
    fs::remove_all(ToPath(root_), error);
    if (error) {
      LOG(ERROR) << "Failed to delete directory \"" << root_
                 << "\". Error: " << error.message();
    }
    return;
  }

  fs::directory_iterator it(ToPath(root_), error);
  if (error) {
    LOG(ERROR) << "Failed to delete directory \"" << root_
               << "\". Error: " << error.message();
  }
  fs::directory_iterator end;
  for (; it != end; it.increment(error)) {
    if (error) {
      LOG(ERROR) << "Failed to delete directory \"" << root_
                 << "\". Error: " << error.message();
    }
    fs::remove_all(it->path(), error);
    if (error) {
      LOG(ERROR) << "Failed to delete \"" << it->path()
                 << "\". Error: " << error.message();
    }
  }
}

FileProtocolFlags LocalFileProtocol::GetFlags() const { return flags_; }

std::vector<std::string> LocalFileProtocol::GetDefaultNames() const {
  return {"file"};
}

PathInfo LocalFileProtocol::DoGetPathInfo(std::string_view protocol_name,
                                          std::string_view path) {
  std::error_code error;
  fs::path full_path = ToPath(JoinPath(root_, path));
  auto status = fs::status(full_path, error);
  if (fs::is_directory(status)) {
    return {PathType::kFolder};
  } else if (fs::is_regular_file(status)) {
    auto size = fs::file_size(full_path, error);
    if (!error) {
      return {PathType::kFile, static_cast<int64_t>(size)};
    }
  }
  return {};
}

template <typename Iterator>
std::vector<std::string> ListFolder(Iterator& it, fs::path& path,
                                    const std::string& root,
                                    std::string_view protocol_name,
                                    std::string_view pattern, PathTypes types) {
  std::error_code error;
  Iterator end;
  std::vector<std::string> entries;
  const std::string prefix = absl::StrCat(protocol_name, ":/");
  for (; it != end; it.increment(error)) {
    if (error) {
      break;
    }
    if (!pattern.empty() &&
        !PathMatchesPattern(RemoveFolder(ToString(it->path())), pattern)) {
      continue;
    }
    auto status = fs::status(it->path(), error);
    if (error) {
      break;
    }
    if ((fs::is_directory(status) && types.IsSet(PathType::kFolder)) ||
        (fs::is_regular_file(status) && types.IsSet(PathType::kFile))) {
      const std::string entry_path = ToString(it->path());
      if (!absl::StartsWith(entry_path, root)) {
        LOG_FIRST_N(ERROR, 1)
            << "Directory iteration returned a string \"" << entry_path
            << "\" which is not under root \"" << root << "\"! Skipping...";
        continue;
      }
      std::string_view relative_path = entry_path;
      relative_path.remove_prefix(root.size());
      entries.emplace_back(JoinPath(prefix, relative_path));
    }
  }
  return entries;
}

std::vector<std::string> LocalFileProtocol::DoList(
    std::string_view protocol_name, std::string_view path,
    std::string_view pattern, FolderMode mode, PathTypes types) {
  std::error_code error;
  fs::path full_path = ToPath(JoinPath(root_, path));

  if (mode == FolderMode::kNormal) {
    fs::directory_iterator it(
        full_path, fs::directory_options::skip_permission_denied, error);
    if (error) {
      return {};
    }
    return ListFolder(it, full_path, root_, protocol_name, pattern, types);
  } else {  // mode == FolderMode::kRecursive
    fs::recursive_directory_iterator it(
        full_path, fs::directory_options::skip_permission_denied, error);
    if (error) {
      return {};
    }
    return ListFolder(it, full_path, root_, protocol_name, pattern, types);
  }
}

bool LocalFileProtocol::DoCreateFolder(std::string_view protocol_name,
                                       std::string_view path, FolderMode mode) {
  std::error_code error;
  fs::path full_path = ToPath(JoinPath(root_, path));

  if (mode == FolderMode::kNormal) {
    auto status = fs::status(full_path, error);
    if (fs::is_directory(status)) {
      return true;
    }
    if (fs::exists(status)) {
      return false;
    }
    if (!fs::create_directory(full_path, error)) {
      return false;
    }
  } else {
    if (!fs::create_directories(full_path, error)) {
      return false;
    }
  }
  return true;
}

bool LocalFileProtocol::DoCopyFolder(std::string_view protocol_name,
                                     std::string_view from_path,
                                     std::string_view to_path) {
  std::error_code error;

  fs::path full_from_path = ToPath(JoinPath(root_, from_path));
  if (!fs::is_directory(full_from_path, error)) {
    return false;
  }

  fs::path full_to_path = ToPath(JoinPath(root_, to_path));
  auto to_status = fs::status(full_to_path, error);
  if (!fs::is_directory(to_status) && fs::exists(to_status)) {
    return false;
  }

  auto options =
      fs::copy_options::overwrite_existing | fs::copy_options::recursive;
  fs::copy(full_from_path, full_to_path, options, error);
  return !error;
}

bool LocalFileProtocol::DoDeleteFolder(std::string_view protocol_name,
                                       std::string_view path, FolderMode mode) {
  if (IsRootPath(path)) {
    return false;
  }

  std::error_code error;
  fs::path full_path = ToPath(JoinPath(root_, path));

  auto status = fs::status(full_path, error);
  if (!fs::is_directory(status)) {
    return !fs::exists(status);
  }

  if (mode == FolderMode::kNormal) {
    return fs::remove(full_path, error);
  } else {  // mode == FolderMode::kRecursive
    return fs::remove_all(full_path, error);
  }
}

bool LocalFileProtocol::DoCopyFile(std::string_view protocol_name,
                                   std::string_view from_path,
                                   std::string_view to_path) {
  std::error_code error;
  fs::path full_from_path = ToPath(JoinPath(root_, from_path));
  fs::path full_to_path = ToPath(JoinPath(root_, to_path));
  return fs::copy_file(full_from_path, full_to_path,
                       fs::copy_options::overwrite_existing, error);
}

bool LocalFileProtocol::DoDeleteFile(std::string_view protocol_name,
                                     std::string_view path) {
  std::error_code error;
  fs::path full_path = ToPath(JoinPath(root_, path));

  auto status = fs::status(full_path, error);
  if (!fs::exists(status)) {
    return true;
  }
  if (!fs::is_regular_file(status)) {
    return false;
  }

  return fs::remove(full_path, error);
}

std::unique_ptr<RawFile> LocalFileProtocol::DoOpenFile(
    std::string_view protocol_name, std::string_view path, FileFlags flags) {
  std::error_code error;
  fs::path full_path = ToPath(JoinPath(root_, path));
  const bool file_exists = fs::is_regular_file(full_path, error);

  if (!flags.IsSet(FileFlag::kCreate) && !file_exists) {
    return nullptr;
  }

  std::fstream::openmode mode = std::fstream::binary;
  if (flags.IsSet(FileFlag::kRead)) {
    mode |= std::fstream::in;
  }
  if (flags.IsSet(FileFlag::kWrite)) {
    mode |= std::fstream::out;
    if (!flags.IsSet(FileFlag::kReset)) {
      // Otherwise it will truncate even without std::fstream::trunc
      mode |= std::fstream::in;
    }
  }
  if (flags.IsSet(FileFlag::kReset) || !file_exists) {
    mode |= std::fstream::trunc;
  }
  std::fstream file(full_path, mode);
  if (file.fail()) {
    return nullptr;
  }
  return std::make_unique<LocalFile>(std::move(file));
}

}  // namespace gb
