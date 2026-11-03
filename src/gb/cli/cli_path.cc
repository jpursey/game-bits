// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_path.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "gb/file/file_types.h"
#include "gb/file/path.h"

namespace gb {

namespace {

// Returns the path in the context's file system of `path`, given on the
// command line: `path` itself, prefixed with the context's protocol if it names
// one and `path` has no protocol of its own.
//
// If the file system doesn't have the protocol that paths given on the command
// line are opened with, this returns a FailedPrecondition error, as that is the
// program's mistake.
absl::StatusOr<std::string> GetFilePath(const ValidatedContext& context,
                                        std::string_view path) {
  const FileSystem* file_system = context.GetPtr<FileSystem>();
  const std::string protocol =
      context.GetValue<std::string>(kKeyCliFileProtocol);
  if (protocol.empty()) {
    if (file_system->GetDefaultProtocolName().empty()) {
      return absl::FailedPreconditionError(
          "paths are opened with the default protocol, which the file system "
          "doesn't have");
    }
  } else if (!file_system->IsRegistered(protocol)) {
    return absl::FailedPreconditionError(
        absl::StrCat("paths are opened with the \"", protocol,
                     "\" protocol, which the file system doesn't have"));
  }
  if (protocol.empty()) {
    return std::string(path);
  }
  std::string path_protocol;
  RemoveProtocol(NormalizePath(path), &path_protocol);
  if (!path_protocol.empty()) {
    return std::string(path);
  }
  return absl::StrCat(protocol, ":", path);
}

// Returns the type of `path`, given on the command line, in the context's file
// system, or an error if it doesn't exist, or the file system doesn't have the
// protocol to open it with.
absl::StatusOr<PathType> GetExistingPathType(const ValidatedContext& context,
                                             std::string_view path) {
  const absl::StatusOr<std::string> file_path = GetFilePath(context, path);
  if (!file_path.ok()) {
    return file_path.status();
  }

  // An empty path would otherwise read as the current folder.
  const PathType type =
      path.empty() ? PathType::kInvalid
                   : context.GetPtr<FileSystem>()->GetPathInfo(*file_path).type;
  if (type == PathType::kInvalid) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", path, "\" doesn't exist"));
  }
  return type;
}

absl::Status ValidatePathExists(const ValidatedContext& context,
                                const std::string_view& path) {
  return GetExistingPathType(context, path).status();
}

absl::Status ValidateFileExists(const ValidatedContext& context,
                                const std::string_view& path) {
  const absl::StatusOr<PathType> type = GetExistingPathType(context, path);
  if (!type.ok()) {
    return type.status();
  }
  if (*type != PathType::kFile) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", path, "\" isn't a file"));
  }
  return absl::OkStatus();
}

absl::Status ValidateFolderExists(const ValidatedContext& context,
                                  const std::string_view& path) {
  const absl::StatusOr<PathType> type = GetExistingPathType(context, path);
  if (!type.ok()) {
    return type.status();
  }
  if (*type != PathType::kFolder) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", path, "\" isn't a folder"));
  }
  return absl::OkStatus();
}

absl::Status ValidateFileWritable(const ValidatedContext& context,
                                  const std::string_view& path) {
  const absl::StatusOr<std::string> path_in_file_system =
      GetFilePath(context, path);
  if (!path_in_file_system.ok()) {
    return path_in_file_system.status();
  }

  // A trailing separator is kept, as it says the path is a folder.
  PathFlags failed_flag;
  const std::string file_path = NormalizePath(
      *path_in_file_system, kGenericPathFlags + PathFlag::kAllowTrailingSlash,
      &failed_flag);
  if (!failed_flag.IsEmpty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", path, "\" isn't a path"));
  }

  // A path that normalizes to nothing ("", "." or "a/..") is the current
  // folder.
  std::string_view filename;
  const std::string_view folder =
      RemoveFilename(std::string_view(file_path), &filename);
  if (file_path.ends_with('/') || filename.empty() || filename == "..") {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", path, "\" has no file name"));
  }
  FileSystem* file_system = context.GetPtr<FileSystem>();
  const PathType type = file_system->GetPathInfo(file_path).type;
  if (type == PathType::kFolder) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", path, "\" is a folder"));
  }

  // Writing a file that doesn't exist yet creates it. A protocol the file
  // system doesn't have has no flags.
  std::string protocol;
  RemoveProtocol(file_path, &protocol);
  if (protocol.empty()) {
    protocol = file_system->GetDefaultProtocolName();
  }
  const FileProtocolFlags flags = file_system->GetFlags(protocol);
  if (!flags.IsSet(FileProtocolFlag::kFileWrite) ||
      (type != PathType::kFile &&
       !flags.IsSet(FileProtocolFlag::kFileCreate))) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", path, "\" can't be written"));
  }

  // An existing file's folder exists. A bare file name's folder is empty, which
  // is the current folder.
  if (type != PathType::kFile && !file_system->IsValidFolder(folder)) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", path, "\"'s folder doesn't exist"));
  }
  return absl::OkStatus();
}

}  // namespace

const CliValidator<std::string_view> kCliPathExists = {
    .validate = ValidatePathExists,
    .constraints = {&kConstraintCliFileSystem, &kConstraintCliFileProtocol},
};

const CliValidator<std::string_view> kCliFileExists = {
    .validate = ValidateFileExists,
    .constraints = {&kConstraintCliFileSystem, &kConstraintCliFileProtocol},
};

const CliValidator<std::string_view> kCliFolderExists = {
    .validate = ValidateFolderExists,
    .constraints = {&kConstraintCliFileSystem, &kConstraintCliFileProtocol},
};

const CliValidator<std::string_view> kCliFileWritable = {
    .validate = ValidateFileWritable,
    .constraints = {&kConstraintCliFileSystem, &kConstraintCliFileProtocol},
};

}  // namespace gb
