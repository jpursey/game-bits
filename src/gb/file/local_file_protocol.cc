// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/local_file_protocol.h"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <random>

#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "gb/base/context_builder.h"
#include "gb/base/stdio_file.h"
#include "gb/base/unicode.h"
#include "gb/file/local_current_path.h"
#include "gb/file/path.h"
#include "gb/file/raw_file.h"

namespace gb {

namespace {

namespace fs = std::filesystem;

class LocalFile : public RawFile {
 public:
  // Takes ownership of `file`.
  explicit LocalFile(std::FILE* file) : file_(file) {}
  ~LocalFile() override { std::fclose(file_); }

  int64_t SeekEnd() override;
  int64_t SeekTo(int64_t position) override;
  int64_t Write(const void* buffer, int64_t size) override;
  bool Flush() override;
  int64_t Read(void* buffer, int64_t size) override;

 private:
  enum class Access { kRead, kWrite };

  // Records the access about to be made, seeking first if the last access was
  // the other kind, as C stdio requires between reading and writing. Returns
  // false if the seek fails.
  bool BeginAccess(Access access);

  std::FILE* const file_;
  Access last_access_ = Access::kRead;
};

bool LocalFile::BeginAccess(Access access) {
  if (last_access_ != access) {
    if (!SeekStdioFile(file_, 0, SEEK_CUR)) {
      return false;
    }
    last_access_ = access;
  }
  return true;
}

int64_t LocalFile::SeekEnd() {
  if (!SeekStdioFile(file_, 0, SEEK_END)) {
    return -1;
  }
  return TellStdioFile(file_);
}

int64_t LocalFile::SeekTo(int64_t position) {
  if (!SeekStdioFile(file_, position, SEEK_SET)) {
    return -1;
  }
  return position;
}

int64_t LocalFile::Write(const void* buffer, int64_t size) {
  if (!BeginAccess(Access::kWrite)) {
    return 0;
  }
  return static_cast<int64_t>(
      std::fwrite(buffer, 1, static_cast<size_t>(size), file_));
}

bool LocalFile::Flush() {
  // Nothing is buffered to write after a read, and C doesn't define fflush()
  // for a stream last read from.
  if (last_access_ == Access::kWrite && std::fflush(file_) != 0) {
    return false;
  }

  // A failed write leaves the error indicator set, so this also reports the
  // errors of earlier writes.
  return !std::ferror(file_);
}

int64_t LocalFile::Read(void* buffer, int64_t size) {
  if (!BeginAccess(Access::kRead)) {
    return -1;
  }
  const int64_t count = static_cast<int64_t>(
      std::fread(buffer, 1, static_cast<size_t>(size), file_));
  if (std::ferror(file_)) {
    return -1;
  }

  // Reaching the end sets the end-of-file indicator, which can stop later
  // reads, so it is cleared to read anything appended to the file since.
  std::clearerr(file_);
  return count;
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

// Returns true if operating system paths start with a drive letter, as Windows
// paths do.
bool HasDriveLetters() {
  static const bool s_has_drive_letters = fs::path("c:/").has_root_name();
  return s_has_drive_letters;
}

// Returns the drive that `protocol_name` names, for a protocol at the operating
// system's root (an empty `root`), or an empty string if it names none: a drive
// is a single lowercase letter, where there are drive letters.
std::string_view GetDrive(std::string_view root,
                          std::string_view protocol_name) {
  if (!root.empty() || !HasDriveLetters() || protocol_name.size() != 1 ||
      !absl::ascii_islower(protocol_name[0])) {
    return {};
  }
  return protocol_name;
}

// Returns the path of a protocol at the operating system's root for `path`, an
// absolute path on `drive`: "/src/a" on "c" is "/c/src/a".
std::string DriveToOsRootPath(std::string_view drive, std::string_view path) {
  return JoinPath(absl::StrCat("/", drive), path);
}

// Returns the operating system path for an absolute path of a protocol at the
// operating system's root, or an empty string if it names no path: where there
// are drive letters, its first folder must be one, in lowercase.
std::string OsRootToFullPath(std::string_view path) {
  if (!HasDriveLetters()) {
    return std::string(path);
  }
  if (path.size() < 2 || !absl::ascii_islower(path[1]) ||
      (path.size() > 2 && path[2] != '/')) {
    return {};
  }
  const std::string_view folder = path.substr(2);
  return absl::StrCat(path.substr(1, 1), ":", folder.empty() ? "/" : folder);
}

// Returns the path of a protocol at the operating system's root for a
// normalized absolute operating system path, or an empty string for a network
// path.
std::string FullToOsRootPath(std::string_view path) {
  std::string_view drive;
  path = RemoveProtocol(path, &drive);
  if (!GetHostName(path).empty()) {
    return {};
  }
  if (drive.empty()) {
    return std::string(path);
  }
  return DriveToOsRootPath(drive, path);
}

// Returns the operating system path for a protocol path under the root (an
// empty root is the operating system's root), or nullopt for an invalid path:
// one with a host, which isn't under the root, or one that isn't valid UTF-8.
std::optional<fs::path> ToFullPath(std::string_view root,
                                   std::string_view path) {
  if (!GetHostName(path).empty()) {
    return std::nullopt;
  }
  fs::path full_path =
      ToPath(root.empty() ? OsRootToFullPath(path) : JoinPath(root, path));
  if (full_path.empty()) {
    return std::nullopt;
  }
  return full_path;
}

// Returns the protocol path for a normalized operating system path, or an
// empty string if it is not under the root (or is a network path, for the
// operating system's root).
std::string ToProtocolPath(std::string_view root, std::string_view path) {
  if (root.empty()) {
    return FullToOsRootPath(path);
  }
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

    // An entry whose status can't be read, such as a link whose target is
    // missing, is neither a file nor a folder.
    auto status = it->status(error);
    if (error) {
      continue;
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

  return absl::WrapUnique(
      new LocalFileProtocol(context.GetValue<FileProtocolFlags>(), root,
                            context.GetValue<bool>(kKeyUniqueRoot),
                            context.GetValue<bool>(kKeyDeleteAtExit)));
}

std::unique_ptr<LocalFileProtocol> LocalFileProtocol::Create(
    const fs::path& root) {
  // An empty root is the current directory, so a root that doesn't convert
  // fails rather than becoming one.
  const std::string root_string = ToString(root);
  if (root_string.empty() && !root.empty()) {
    LOG(ERROR) << "Requested root path could not be converted to a UTF-8 path";
    return nullptr;
  }
  return Create(
      ContextBuilder().SetValue<std::string>(kKeyRoot, root_string).Build());
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

std::unique_ptr<LocalFileProtocol> LocalFileProtocol::CreateOsRoot() {
  return absl::WrapUnique(
      new LocalFileProtocol(kDefaultFlags, {}, false, false));
}

std::string LocalFileProtocol::ToOsRootPath(std::string_view native_path) {
  const fs::path path = ToPath(native_path);
  if (path.empty()) {
    return {};
  }
  std::error_code error;
  const fs::path absolute_path = fs::absolute(path, error);
  if (error) {
    return {};
  }
  return FullToOsRootPath(ToString(absolute_path));
}

LocalFileProtocol::LocalFileProtocol(FileProtocolFlags flags,
                                     std::string_view root, bool unique_root,
                                     bool delete_at_exit)
    : flags_(flags),
      root_(root),
      unique_root_(unique_root),
      delete_at_exit_(delete_at_exit) {}

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
  std::vector<std::string> names = {"file"};
  if (root_.empty() && HasDriveLetters()) {
    for (char drive = 'a'; drive <= 'z'; ++drive) {
      names.emplace_back(1, drive);
    }
  }
  return names;
}

std::string LocalFileProtocol::GetCanonicalPath(std::string_view protocol_name,
                                                std::string_view path) const {
  const std::string_view drive = GetDrive(root_, protocol_name);
  if (drive.empty()) {
    return std::string(path);
  }
  return DriveToOsRootPath(drive, path);
}

std::optional<std::filesystem::path> LocalFileProtocol::GetFullPath(
    std::string_view protocol_name, std::string_view path) const {
  return ToFullPath(GetNativeRoot(protocol_name), path);
}

std::string LocalFileProtocol::GetNativeRoot(
    std::string_view protocol_name) const {
  const std::string_view drive = GetDrive(root_, protocol_name);
  if (drive.empty()) {
    return root_;
  }
  return absl::StrCat(drive, ":/");
}

PathInfo LocalFileProtocol::DoGetPathInfo(std::string_view protocol_name,
                                          std::string_view path) {
  auto full_path = GetFullPath(protocol_name, path);
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
  auto full_path = GetFullPath(protocol_name, path);
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
    return ListFolder(std::move(it), GetNativeRoot(protocol_name),
                      protocol_name, pattern, types);
  } else {  // mode == FolderMode::kRecursive
    fs::recursive_directory_iterator it(
        *full_path, fs::directory_options::skip_permission_denied, error);
    if (error) {
      return {};
    }
    return ListFolder(std::move(it), GetNativeRoot(protocol_name),
                      protocol_name, pattern, types);
  }
}

bool LocalFileProtocol::DoCreateFolder(std::string_view protocol_name,
                                       std::string_view path, FolderMode mode) {
  auto full_path = GetFullPath(protocol_name, path);
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

bool LocalFileProtocol::DoCopyFolder(std::string_view from_protocol_name,
                                     std::string_view from_path,
                                     std::string_view to_protocol_name,
                                     std::string_view to_path) {
  auto full_from_path = GetFullPath(from_protocol_name, from_path);
  auto full_to_path = GetFullPath(to_protocol_name, to_path);
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
  // Neither the protocol's root nor a drive's root folder is deleted.
  auto full_path = GetFullPath(protocol_name, path);
  if (!full_path || IsRootPath(path) || !full_path->has_relative_path()) {
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

bool LocalFileProtocol::DoCopyFile(std::string_view from_protocol_name,
                                   std::string_view from_path,
                                   std::string_view to_protocol_name,
                                   std::string_view to_path) {
  auto full_from_path = GetFullPath(from_protocol_name, from_path);
  auto full_to_path = GetFullPath(to_protocol_name, to_path);
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
  auto full_path = GetFullPath(protocol_name, path);
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
  auto full_path = GetFullPath(protocol_name, path);
  if (!full_path) {
    return nullptr;
  }

  std::error_code error;
  const bool file_exists = fs::is_regular_file(*full_path, error);

  if (!flags.IsSet(FileFlag::kCreate) && !file_exists) {
    return nullptr;
  }

  // C stdio's "w" modes create or empty the file, and its "r" modes need the
  // file to exist. Only "r+" writes without emptying it.
  const char* mode = "rb";
  if (flags.IsSet(FileFlag::kWrite)) {
    if (flags.IsSet(FileFlag::kReset) || !file_exists) {
      mode = flags.IsSet(FileFlag::kRead) ? "w+b" : "wb";
    } else {
      mode = "r+b";
    }
  }
  std::FILE* file = OpenStdioFile(*full_path, mode);
  if (file == nullptr) {
    return nullptr;
  }
  return std::make_unique<LocalFile>(file);
}

std::string LocalFileProtocol::DoGetCurrentPath(
    std::string_view protocol_name) {
  // The operating system keeps a current folder for each drive, which a drive
  // on its own ("c:") resolves to.
  const std::string_view drive = GetDrive(root_, protocol_name);
  std::error_code error;
  const fs::path fs_current_path =
      drive.empty() ? fs::current_path(error)
                    : fs::absolute(ToPath(absl::StrCat(drive, ":")), error);
  if (error) {
    return {};
  }
  return ToProtocolPath(GetNativeRoot(protocol_name),
                        ToString(fs_current_path));
}

bool LocalFileProtocol::DoSetCurrentPath(std::string_view protocol_name,
                                         std::string_view path) {
  auto full_path = GetFullPath(protocol_name, path);
  if (!full_path) {
    return false;
  }

  return internal::SetLocalCurrentPath(*full_path);
}

}  // namespace gb
