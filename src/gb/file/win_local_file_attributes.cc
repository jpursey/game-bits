// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/local_file_attributes.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb::internal {

namespace {

// Opens the file or folder at `path`, following links, with `access`.
HANDLE OpenPath(const std::filesystem::path& path, DWORD access) {
  return ::CreateFileW(path.c_str(), access,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS,
                       nullptr);
}

FileAttributes ToFileAttributes(DWORD windows_attributes) {
  FileAttributes attributes;
  if ((windows_attributes & FILE_ATTRIBUTE_READONLY) != 0) {
    attributes += FileAttribute::kReadOnly;
  }
  if ((windows_attributes & FILE_ATTRIBUTE_HIDDEN) != 0) {
    attributes += FileAttribute::kHidden;
  }
  return attributes;
}

}  // namespace

FileAttributes GetLocalAttributeFlags() {
  return {FileAttribute::kReadOnly, FileAttribute::kHidden};
}

FileAttributes GetLocalAttributes(const std::filesystem::path& path) {
  const DWORD windows_attributes = ::GetFileAttributesW(path.c_str());
  if (windows_attributes == INVALID_FILE_ATTRIBUTES) {
    return {};
  }

  // A link (a reparse point) reports its own attributes, so what it links to
  // is opened to read its attributes instead.
  if ((windows_attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
    return ToFileAttributes(windows_attributes);
  }
  const HANDLE handle = OpenPath(path, FILE_READ_ATTRIBUTES);
  if (handle == INVALID_HANDLE_VALUE) {
    return {};
  }
  FILE_BASIC_INFO info = {};
  const bool success = ::GetFileInformationByHandleEx(handle, FileBasicInfo,
                                                      &info, sizeof(info));
  ::CloseHandle(handle);
  if (!success) {
    return {};
  }
  return ToFileAttributes(info.FileAttributes);
}

bool SetLocalAttributes(const std::filesystem::path& path,
                        FileAttributes attributes) {
  const HANDLE handle =
      OpenPath(path, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES);
  if (handle == INVALID_HANDLE_VALUE) {
    return false;
  }
  FILE_BASIC_INFO info = {};
  bool success = ::GetFileInformationByHandleEx(handle, FileBasicInfo, &info,
                                                sizeof(info));
  if (success) {
    // Every other attribute is kept as it is, and those that can't be set
    // (such as FILE_ATTRIBUTE_DIRECTORY) are ignored. FILE_ATTRIBUTE_NORMAL
    // means none, so it is only set alone. Zero times are left unchanged, and
    // zero attributes would be too.
    DWORD windows_attributes =
        info.FileAttributes & ~(FILE_ATTRIBUTE_READONLY |
                                FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NORMAL);
    if (attributes.IsSet(FileAttribute::kReadOnly)) {
      windows_attributes |= FILE_ATTRIBUTE_READONLY;
    }
    if (attributes.IsSet(FileAttribute::kHidden)) {
      windows_attributes |= FILE_ATTRIBUTE_HIDDEN;
    }
    if (windows_attributes == 0) {
      windows_attributes = FILE_ATTRIBUTE_NORMAL;
    }
    info = {};
    info.FileAttributes = windows_attributes;
    success = ::SetFileInformationByHandle(handle, FileBasicInfo, &info,
                                           sizeof(info));
  }
  ::CloseHandle(handle);
  return success;
}

}  // namespace gb::internal
