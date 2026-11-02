// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_path.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "gb/file/file_types.h"
#include "gb/file/local_file_protocol.h"

namespace gb {

namespace {

// Validates that the context's file system has the protocol that paths given on
// the command line are opened with. A missing one is the program's mistake, so
// this returns a FailedPrecondition error.
absl::Status ValidateProtocol(const ValidatedContext& context) {
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
  return absl::OkStatus();
}

// Returns the type of `path`, given on the command line, in the context's file
// system, or an error if it doesn't exist or can't be converted, or the file
// system doesn't have the protocol to open it with.
absl::StatusOr<PathType> GetExistingPathType(const ValidatedContext& context,
                                             std::string_view path) {
  const absl::Status status = ValidateProtocol(context);
  if (!status.ok()) {
    return status;
  }
  const std::string file_path = GetCliFilePath(context, path);
  const PathType type =
      file_path.empty()
          ? PathType::kInvalid
          : context.GetPtr<FileSystem>()->GetPathInfo(file_path).type;
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

}  // namespace

std::string GetCliFilePath(const ValidatedContext& context,
                           std::string_view path) {
  const std::string os_root_path = LocalFileProtocol::ToOsRootPath(path);
  const std::string protocol =
      context.GetValue<std::string>(kKeyCliFileProtocol);
  if (os_root_path.empty() || protocol.empty()) {
    return os_root_path;
  }
  return absl::StrCat(protocol, ":", os_root_path);
}

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

}  // namespace gb
