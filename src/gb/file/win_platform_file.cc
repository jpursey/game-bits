// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <direct.h>

#include <string>
#include <string_view>

#include "absl/algorithm/container.h"
#include "absl/strings/ascii.h"
#include "gb/base/unicode.h"
#include "gb/file/platform_file.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb::internal {

namespace {

namespace fs = std::filesystem;

// Windows file times count 100 nanosecond ticks from 1601-01-01 UTC, as
// FILETIME does. std::chrono's own conversion goes through utc_clock, which
// loads the time zone database to look up leap seconds.
constexpr absl::Duration kFileTimeTick = absl::Nanoseconds(100);
constexpr absl::Time kFileTimeEpoch = absl::FromUnixSeconds(-11644473600);

// Names Windows opens as devices, in lowercase, apart from the COM and LPT
// ports.
constexpr std::string_view kDeviceNames[] = {"con", "prn",    "aux",
                                             "nul", "conin$", "conout$"};

// Returns true if Windows opens a file or folder named `name` as a device: if,
// in any case, its part before the first '.' or ':', less trailing spaces, is a
// device name. Windows 11 no longer reads a name with an extension as a device,
// but earlier versions do, so it is one here on every version.
bool IsDeviceName(const fs::path& name) {
  const std::string lower = absl::AsciiStrToLower(LocalPathToUtf8(name));
  std::string_view base =
      std::string_view(lower).substr(0, lower.find_first_of(".:"));
  while (!base.empty() && base.back() == ' ') {
    base.remove_suffix(1);
  }

  // Ports are numbered with a digit, or a superscript 1, 2, or 3 (in UTF-8).
  if (base.starts_with("com") || base.starts_with("lpt")) {
    const std::string_view number = base.substr(3);
    return (number.size() == 1 && absl::ascii_isdigit(number[0])) ||
           number == "\xC2\xB9" || number == "\xC2\xB2" || number == "\xC2\xB3";
  }
  return absl::c_linear_search(kDeviceNames, base);
}

// Opens the file or folder at `path`, following links, with `access`.
HANDLE OpenPath(const fs::path& path, DWORD access) {
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

//------------------------------------------------------------------------------
// Path encoding
//
// Windows paths are UTF-16.
//------------------------------------------------------------------------------

std::string LocalPathToUtf8(const fs::path& path) {
  return ToUtf8(path.generic_u16string());
}

fs::path Utf8ToLocalPath(std::string_view path) {
  return fs::path(ToUtf16(path), fs::path::generic_format);
}

// Names are UTF-16 code units that aren't checked, so a lone surrogate is
// accepted.
fs::path GetNonUtf8Name() { return fs::path(std::wstring(1, wchar_t{0xD800})); }

//------------------------------------------------------------------------------
// Device names
//------------------------------------------------------------------------------

bool HasLocalDeviceName(const fs::path& path) {
  return absl::c_any_of(path, IsDeviceName);
}

//------------------------------------------------------------------------------
// Current path
//------------------------------------------------------------------------------

bool SetLocalCurrentPath(const fs::path& path) {
  // Unlike SetCurrentDirectoryW(), this also sets the drive's current folder
  // (the "=C:" environment variable), which a drive on its own ("c:")
  // resolves to while another drive is current.
  return _wchdir(path.c_str()) == 0;
}

//------------------------------------------------------------------------------
// Attributes
//------------------------------------------------------------------------------

FileAttributes GetLocalAttributeFlags() {
  return {FileAttribute::kReadOnly, FileAttribute::kHidden};
}

FileAttributes GetLocalAttributes(const fs::path& path) {
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

bool SetLocalAttributes(const fs::path& path, FileAttributes attributes) {
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

//------------------------------------------------------------------------------
// File times
//------------------------------------------------------------------------------

absl::Time FromLocalFileTime(fs::file_time_type time) {
  return kFileTimeEpoch + kFileTimeTick * time.time_since_epoch().count();
}

fs::file_time_type ToLocalFileTime(absl::Time time) {
  return fs::file_time_type(
      fs::file_time_type::duration((time - kFileTimeEpoch) / kFileTimeTick));
}

}  // namespace gb::internal
