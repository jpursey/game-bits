// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_PLATFORM_FILE_H_
#define GB_FILE_PLATFORM_FILE_H_

#include <filesystem>
#include <string>
#include <string_view>

#include "absl/time/time.h"
#include "gb/file/file_types.h"

namespace gb::internal {

//==============================================================================
// What LocalFileProtocol needs from the operating system beyond
// std::filesystem, implemented once per platform.
//==============================================================================

//------------------------------------------------------------------------------
// Path encoding
//
// Converts between UTF-8 paths and std::filesystem paths, in the encoding the
// operating system's paths use. std::filesystem's own conversions throw for a
// string that isn't valid, so these are used instead.
//------------------------------------------------------------------------------

// Returns `path` in generic format (with '/' separators) as UTF-8, or an empty
// string if it isn't valid in the operating system's encoding.
std::string LocalPathToUtf8(const std::filesystem::path& path);

// Returns the UTF-8 `path` as a std::filesystem path, read in generic format,
// or an empty path if `path` isn't valid UTF-8.
std::filesystem::path Utf8ToLocalPath(std::string_view path);

// Returns a file name that the operating system accepts but that can't be
// converted to UTF-8, so no UTF-8 path can name it. Tests use it to check how
// such names are handled.
std::filesystem::path GetNonUtf8Name();

//------------------------------------------------------------------------------
// Device names
//------------------------------------------------------------------------------

// Returns true if any name in `path` is one the operating system opens as a
// device rather than as a file or folder, in whatever folder it is.
bool HasLocalDeviceName(const std::filesystem::path& path);

//------------------------------------------------------------------------------
// Current path
//------------------------------------------------------------------------------

// Sets the process's current folder to `path`, returning false if it can't.
//
// Where the operating system keeps a current folder for each drive, this also
// sets it for the drive of `path`, as a command shell does, so it is still the
// drive's current folder once another drive's is set.
bool SetLocalCurrentPath(const std::filesystem::path& path);

//------------------------------------------------------------------------------
// Attributes
//------------------------------------------------------------------------------

// Returns the attributes local files support on this operating system.
FileAttributes GetLocalAttributeFlags();

// Returns the attributes of the file or folder at `path`, following links, or
// an empty set if they can't be read.
FileAttributes GetLocalAttributes(const std::filesystem::path& path);

// Sets the attributes of the file or folder at `path`, following links, to
// `attributes`, which must all be in GetLocalAttributeFlags(). Returns false if
// it can't.
bool SetLocalAttributes(const std::filesystem::path& path,
                        FileAttributes attributes);

//------------------------------------------------------------------------------
// File times
//
// Converts between std::filesystem file times and absl::Time. File times keep
// the precision of the standard library's file clock, so a time converted to
// one is rounded down to it.
//------------------------------------------------------------------------------

absl::Time FromLocalFileTime(std::filesystem::file_time_type time);
std::filesystem::file_time_type ToLocalFileTime(absl::Time time);

}  // namespace gb::internal

#endif  // GB_FILE_PLATFORM_FILE_H_
