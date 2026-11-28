// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <chrono>
#include <string>
#include <system_error>

#include "gb/base/unicode.h"
#include "gb/file/platform_file.h"

namespace gb::internal {

namespace {

namespace fs = std::filesystem;

using std::chrono::file_clock;
using std::chrono::system_clock;

constexpr fs::perms kWritePerms =
    fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;

bool IsValidUtf8(std::string_view string) {
  switch (GetStringEncoding(string)) {
    case StringEncoding::kAscii:
    case StringEncoding::kUtf8:
    case StringEncoding::kUtf8WithBom:
      return true;
    default:
      return false;
  }
}

}  // namespace

//------------------------------------------------------------------------------
// Path encoding
//
// Paths are bytes, which are taken as UTF-8.
//------------------------------------------------------------------------------

std::string LocalPathToUtf8(const fs::path& path) {
  std::string string = path.generic_string();
  if (!IsValidUtf8(string)) {
    return {};
  }
  return string;
}

fs::path Utf8ToLocalPath(std::string_view path) {
  if (!IsValidUtf8(path)) {
    return {};
  }
  return fs::path(path, fs::path::generic_format);
}

// GetNonUtf8Name() is in posix_platform_file.cc and web_platform_file.cc, as
// the web has no such name.

//------------------------------------------------------------------------------
// Allowed names
//
// Any name is allowed. Devices have paths of their own (such as "/dev/null"),
// which aren't regular files, rather than names that are devices in every
// folder.
//------------------------------------------------------------------------------

bool IsAllowedLocalPath(std::string_view path) { return true; }

//------------------------------------------------------------------------------
// Current path
//------------------------------------------------------------------------------

bool SetLocalCurrentPath(const fs::path& path) {
  std::error_code error;
  fs::current_path(path, error);
  return !error;
}

//------------------------------------------------------------------------------
// Attributes
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// File times
//------------------------------------------------------------------------------

absl::Time FromLocalFileTime(fs::file_time_type time) {
  return absl::FromChrono(std::chrono::time_point_cast<system_clock::duration>(
      file_clock::to_sys(time)));
}

fs::file_time_type ToLocalFileTime(absl::Time time) {
  return std::chrono::time_point_cast<file_clock::duration>(
      file_clock::from_sys(absl::ToChronoTime(time)));
}

}  // namespace gb::internal
