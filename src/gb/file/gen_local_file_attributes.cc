// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <system_error>

#include "gb/file/local_file_attributes.h"

namespace gb::internal {

namespace {

namespace fs = std::filesystem;

constexpr fs::perms kWritePerms =
    fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;

}  // namespace

// Hidden is a name starting with ".", which isn't an attribute that can be set.
FileAttributes GetLocalAttributeFlags() { return FileAttribute::kReadOnly; }

// Read-only is no write permission for anyone.
FileAttributes GetLocalAttributes(const fs::path& path) {
  std::error_code error;
  const fs::file_status status = fs::status(path, error);
  if (error) {
    return {};
  }
  if ((status.permissions() & kWritePerms) == fs::perms::none) {
    return FileAttribute::kReadOnly;
  }
  return {};
}

// Clearing read-only gives only the owner write permission.
bool SetLocalAttributes(const fs::path& path, FileAttributes attributes) {
  std::error_code error;
  if (attributes.IsSet(FileAttribute::kReadOnly)) {
    fs::permissions(path, kWritePerms, fs::perm_options::remove, error);
  } else if (GetLocalAttributes(path).IsSet(FileAttribute::kReadOnly)) {
    fs::permissions(path, fs::perms::owner_write, fs::perm_options::add, error);
  }
  return !error;
}

}  // namespace gb::internal
