// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_path.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gb/file/file_types.h"
#include "gb/file/local_file_protocol.h"

namespace gb {

namespace {

// Returns the type of `path`, given on the command line, in the context's file
// system, which is PathType::kInvalid if it doesn't exist or can't be
// converted.
PathType GetPathType(const ValidatedContext& context, std::string_view path) {
  const std::string file_path = GetCliFilePath(context, path);
  if (file_path.empty()) {
    return PathType::kInvalid;
  }
  return context.GetPtr<FileSystem>()->GetPathInfo(file_path).type;
}

absl::Status PathDoesNotExist(std::string_view path) {
  return absl::InvalidArgumentError(
      absl::StrCat("\"", path, "\" doesn't exist"));
}

absl::Status ValidatePathExists(const ValidatedContext& context,
                                const std::string_view& path) {
  if (GetPathType(context, path) == PathType::kInvalid) {
    return PathDoesNotExist(path);
  }
  return absl::OkStatus();
}

absl::Status ValidateFileExists(const ValidatedContext& context,
                                const std::string_view& path) {
  const PathType type = GetPathType(context, path);
  if (type == PathType::kInvalid) {
    return PathDoesNotExist(path);
  }
  if (type != PathType::kFile) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", path, "\" isn't a file"));
  }
  return absl::OkStatus();
}

absl::Status ValidateFolderExists(const ValidatedContext& context,
                                  const std::string_view& path) {
  const PathType type = GetPathType(context, path);
  if (type == PathType::kInvalid) {
    return PathDoesNotExist(path);
  }
  if (type != PathType::kFolder) {
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
