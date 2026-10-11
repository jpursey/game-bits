// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/local_file_protocol.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "gb/base/context_builder.h"
#include "gb/base/unicode.h"
#include "gb/file/path.h"
#include "gb/file/raw_file.h"

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
  enum class Access { kRead, kWrite };

  int64_t GetPosition();

  // Records the access about to be made, seeking first if the last access was
  // the other kind: like C's FILE, the stream needs a seek between reading and
  // writing.
  void BeginAccess(Access access);

  std::fstream file_;
  int64_t position_ = 0;
  Access last_access_ = Access::kRead;
};

void LocalFile::BeginAccess(Access access) {
  if (last_access_ != access) {
    file_.seekg(static_cast<std::fstream::pos_type>(position_));
    last_access_ = access;
  }
}

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
  BeginAccess(Access::kWrite);
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
  BeginAccess(Access::kRead);
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

// Paths are UTF-8, which std::filesystem doesn't assume for char strings, and
// its own UTF-8 conversion throws for an invalid string. These convert through
// UTF-16 instead, which gives an empty string for an invalid one.
std::string ToString(const fs::path& path) {
  return NormalizePath(ToUtf8(path.generic_u16string()));
}

fs::path ToPath(std::string_view path) {
  return fs::path(ToUtf16(path), fs::path::generic_format);
}

// Returns the operating system path for a protocol path under the root, or
// nullopt for an invalid path: one with a host, which isn't under the root, or
// one that isn't valid UTF-8.
std::optional<fs::path> ToFullPath(std::string_view root,
                                   std::string_view path) {
  if (!GetHostName(path).empty()) {
    return std::nullopt;
  }
  fs::path full_path = ToPath(JoinPath(root, path));
  if (full_path.empty()) {
    return std::nullopt;
  }
  return full_path;
}

// Returns the protocol path for a normalized operating system path, or an
// empty string if it is not under the root.
std::string ToProtocolPath(std::string_view root, std::string_view path) {
  if (!absl::ConsumePrefix(&path, root)) {
    return {};
  }

  // Only a root folder (such as "c:/") ends in a separator, so the rest of a
  // path under any other root starts with one.
  if (root.back() != '/' && !path.empty() && !absl::ConsumePrefix(&path, "/")) {
    return {};
  }
  return absl::StrCat("/", path);
}

template <typename Iterator>
std::vector<std::string> ListFolder(Iterator it, std::string_view root,
                                    std::string_view protocol_name,
                                    std::string_view pattern, PathTypes types) {
  std::error_code error;
  std::vector<std::string> entries;
  for (const Iterator end; it != end; it.increment(error)) {
    const std::string entry_path = ToString(it->path());
    if (!pattern.empty() &&
        !PathMatchesPattern(RemoveFolder(entry_path), pattern)) {
      continue;
    }
    auto status = it->status(error);
    if (error) {
      break;
    }
    if ((fs::is_directory(status) && types.IsSet(PathType::kFolder)) ||
        (fs::is_regular_file(status) && types.IsSet(PathType::kFile))) {
      const std::string protocol_path = ToProtocolPath(root, entry_path);
      if (protocol_path.empty()) {
        LOG_FIRST_N(ERROR, 1)
            << "Directory iteration returned a path \"" << entry_path
            << "\" which is not a valid path under root \"" << root
            << "\"! Skipping...";
        continue;
      }
      entries.emplace_back(absl::StrCat(protocol_name, ":", protocol_path));
    }
  }
  return entries;
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

  // Get the current path if we can
  std::string current_path;
  if (auto current_path_fs = fs::current_path(error); !error) {
    current_path = ToString(current_path_fs);
  }

  // Change relative paths to be relative to the current working directory for
  // this process.
  const std::string requested_root = context.GetValue<std::string>(kKeyRoot);
  PathFlags failed_flag;
  std::string root = NormalizePath(requested_root, &failed_flag);
  if (!failed_flag.IsEmpty()) {
    LOG(ERROR) << "Requested root path \"" << requested_root
               << "\" could not be normalized";
    return nullptr;
  }
  if (!IsPathAbsolute(root)) {
    if (current_path.empty()) {
      LOG(ERROR) << "Cannot access current directory when resolving requested "
                    "relative root path \""
                 << root << "\". Error: " << error.message();
      return nullptr;
    }
    root = NormalizePath(JoinPath(current_path, root));
    if (root.empty()) {
      LOG(ERROR) << "Requested root path \"" << requested_root
                 << "\" could not be resolved against current directory \""
                 << current_path << "\"";
      return nullptr;
    }
  } else {
    // On Windows, drive letters (aka protocols) are a thing. If a protocol
    // was specified we need to honor it, and if it wasn't specified, the root
    // is on the drive of the current path.
    std::string_view root_protocol_name;
    RemoveProtocol(root, &root_protocol_name);
    if (root_protocol_name.empty()) {
      std::string_view current_protocol_name;
      RemoveProtocol(current_path, &current_protocol_name);
      if (!current_protocol_name.empty()) {
        root = absl::StrCat(current_protocol_name, ":", root);
      }
    }
  }

  // Determine the root directory and requested sub directory to make into the
  // root, if there is one.
  std::string sub_directory;
  auto root_status = fs::status(ToPath(root), error);
  if (!fs::status_known(root_status)) {
    LOG(ERROR) << "Error accessing requested root path \"" << requested_root
               << "\". Error: " << error.message();
    return nullptr;
  } else if (!fs::exists(root_status)) {
    root = RemoveFilename(root, &sub_directory);
    auto parent_status = fs::status(ToPath(root), error);
    if (!fs::status_known(parent_status)) {
      LOG(ERROR) << "Could not access parent path of root path \""
                 << requested_root << "\". Error: " << error.message();
      return nullptr;
    }
    if (!fs::is_directory(parent_status)) {
      LOG(ERROR) << "Requested root path \"" << requested_root
                 << "\" is not relative to a valid directory.";
      return nullptr;
    }
  } else if (!fs::is_directory(root_status)) {
    LOG(ERROR) << "Requested root path \"" << requested_root
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

  // An error from the iterator ends it, so it is reported after the loop.
  fs::directory_iterator it(ToPath(root_), error);
  for (const fs::directory_iterator end; it != end; it.increment(error)) {
    std::error_code remove_error;
    fs::remove_all(it->path(), remove_error);
    if (remove_error) {
      LOG(ERROR) << "Failed to delete \"" << ToString(it->path())
                 << "\". Error: " << remove_error.message();
    }
  }
  if (error) {
    LOG(ERROR) << "Failed to delete directory \"" << root_
               << "\". Error: " << error.message();
  }
}

FileProtocolFlags LocalFileProtocol::GetFlags() const { return flags_; }

std::vector<std::string> LocalFileProtocol::GetDefaultNames() const {
  return {"file"};
}

PathInfo LocalFileProtocol::DoGetPathInfo(std::string_view protocol_name,
                                          std::string_view path) {
  auto full_path = ToFullPath(root_, path);
  if (!full_path) {
    return {};
  }

  std::error_code error;
  auto status = fs::status(*full_path, error);
  if (fs::is_directory(status)) {
    return {PathType::kFolder};
  } else if (fs::is_regular_file(status)) {
    auto size = fs::file_size(*full_path, error);
    if (!error) {
      return {PathType::kFile, static_cast<int64_t>(size)};
    }
  }
  return {};
}

std::vector<std::string> LocalFileProtocol::DoList(
    std::string_view protocol_name, std::string_view path,
    std::string_view pattern, FolderMode mode, PathTypes types) {
  auto full_path = ToFullPath(root_, path);
  if (!full_path) {
    return {};
  }

  std::error_code error;
  if (mode == FolderMode::kNormal) {
    fs::directory_iterator it(
        *full_path, fs::directory_options::skip_permission_denied, error);
    if (error) {
      return {};
    }
    return ListFolder(std::move(it), root_, protocol_name, pattern, types);
  } else {  // mode == FolderMode::kRecursive
    fs::recursive_directory_iterator it(
        *full_path, fs::directory_options::skip_permission_denied, error);
    if (error) {
      return {};
    }
    return ListFolder(std::move(it), root_, protocol_name, pattern, types);
  }
}

bool LocalFileProtocol::DoCreateFolder(std::string_view protocol_name,
                                       std::string_view path, FolderMode mode) {
  auto full_path = ToFullPath(root_, path);
  if (!full_path) {
    return false;
  }

  std::error_code error;
  auto status = fs::status(*full_path, error);
  if (fs::is_directory(status)) {
    return true;
  }
  if (fs::exists(status)) {
    return false;
  }
  if (mode == FolderMode::kNormal) {
    return fs::create_directory(*full_path, error);
  } else {  // mode == FolderMode::kRecursive
    return fs::create_directories(*full_path, error);
  }
}

bool LocalFileProtocol::DoCopyFolder(std::string_view protocol_name,
                                     std::string_view from_path,
                                     std::string_view to_path) {
  auto full_from_path = ToFullPath(root_, from_path);
  auto full_to_path = ToFullPath(root_, to_path);
  if (!full_from_path || !full_to_path) {
    return false;
  }

  std::error_code error;
  if (!fs::is_directory(*full_from_path, error)) {
    return false;
  }
  auto to_status = fs::status(*full_to_path, error);
  if (!fs::is_directory(to_status) && fs::exists(to_status)) {
    return false;
  }

  auto options =
      fs::copy_options::overwrite_existing | fs::copy_options::recursive;
  fs::copy(*full_from_path, *full_to_path, options, error);
  return !error;
}

bool LocalFileProtocol::DoDeleteFolder(std::string_view protocol_name,
                                       std::string_view path, FolderMode mode) {
  auto full_path = ToFullPath(root_, path);
  if (!full_path || IsRootPath(path)) {
    return false;
  }

  std::error_code error;
  auto status = fs::status(*full_path, error);
  if (!fs::is_directory(status)) {
    return !fs::exists(status);
  }

  if (mode == FolderMode::kNormal) {
    return fs::remove(*full_path, error);
  } else {  // mode == FolderMode::kRecursive
    fs::remove_all(*full_path, error);
    return !error;
  }
}

bool LocalFileProtocol::DoCopyFile(std::string_view protocol_name,
                                   std::string_view from_path,
                                   std::string_view to_path) {
  auto full_from_path = ToFullPath(root_, from_path);
  auto full_to_path = ToFullPath(root_, to_path);
  if (!full_from_path || !full_to_path) {
    return false;
  }

  // std::filesystem fails to copy a file onto itself, which another path can
  // also name (Windows paths ignore case).
  std::error_code error;
  if (fs::equivalent(*full_from_path, *full_to_path, error)) {
    return fs::is_regular_file(*full_from_path, error);
  }
  return fs::copy_file(*full_from_path, *full_to_path,
                       fs::copy_options::overwrite_existing, error);
}

bool LocalFileProtocol::DoDeleteFile(std::string_view protocol_name,
                                     std::string_view path) {
  auto full_path = ToFullPath(root_, path);
  if (!full_path) {
    return false;
  }

  std::error_code error;
  auto status = fs::status(*full_path, error);
  if (!fs::exists(status)) {
    return true;
  }
  if (!fs::is_regular_file(status)) {
    return false;
  }

  return fs::remove(*full_path, error);
}

std::unique_ptr<RawFile> LocalFileProtocol::DoOpenFile(
    std::string_view protocol_name, std::string_view path, FileFlags flags) {
  auto full_path = ToFullPath(root_, path);
  if (!full_path) {
    return nullptr;
  }

  std::error_code error;
  const bool file_exists = fs::is_regular_file(*full_path, error);

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
  std::fstream file(*full_path, mode);
  if (file.fail()) {
    return nullptr;
  }
  return std::make_unique<LocalFile>(std::move(file));
}

std::string LocalFileProtocol::DoGetCurrentPath(
    std::string_view protocol_name) {
  std::error_code error;
  auto fs_current_path = fs::current_path(error);
  if (error) {
    return {};
  }
  return ToProtocolPath(root_, ToString(fs_current_path));
}

bool LocalFileProtocol::DoSetCurrentPath(std::string_view protocol_name,
                                         std::string_view path) {
  auto full_path = ToFullPath(root_, path);
  if (!full_path) {
    return false;
  }

  std::error_code error;
  fs::current_path(*full_path, error);
  return !error;
}

}  // namespace gb
