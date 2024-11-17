// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_FILE_FILE_TYPES_H_
#define GB_BASE_FILE_FILE_TYPES_H_

#include <stdint.h>

#include <string_view>

#include "gb/base/flags.h"

namespace gb {

class ChunkReader;
class ChunkWriter;
class File;
class FileProtocol;
class FileSystem;
class RawFile;
struct ChunkHeader;
struct ChunkType;

// Defines what capabilities are available for a given file protocol.
enum class FileProtocolFlag {
  kInfo,          // Supports retrieving path info. Most protocols should
                  // support this, unless it is impractical.
  kList,          // Supports listing existing files (and folders). If this is
                  // supported, kInfo must also be supported.
  kFolderCreate,  // Supports creating and deleting folders. If this is
                  // supported, then kFileCreate must also be supported.
  kFileCreate,    // Supports creating and deleting files. If this is supported,
                  // then kFileWrite must also be supported.
  kFileRead,      // Supports reading files. Protocols must support this
                  // and/or kFileWrite.
  kFileWrite,     // Supports writing files. Protocols must support this
                  // and/or kFileRead.
  kCurrentPath,   // Supports getting and setting the current path. If this is
                  // supported, then the FileSystem will support relative paths
                  // with this protocol.
};
using FileProtocolFlags = Flags<FileProtocolFlag>;

// Protocol supporting all file and folder reead/write features.
inline constexpr FileProtocolFlags kReadWriteFileProtocolFlags = {
    FileProtocolFlag::kInfo,         FileProtocolFlag::kList,
    FileProtocolFlag::kFolderCreate, FileProtocolFlag::kFileCreate,
    FileProtocolFlag::kFileRead,     FileProtocolFlag::kFileWrite,
};

// Typical read-only file protocol.
inline constexpr FileProtocolFlags kReadOnlyFileProtocolFlags = {
    FileProtocolFlag::kInfo,
    FileProtocolFlag::kList,
    FileProtocolFlag::kFileRead,
};

// Defines the behavior for functions that can operate over folders recursively
// or not.
enum class FolderMode {
  kNormal,     // No recursion is done.
  kRecursive,  // Operation operates on files and folders recursively.
};

// Defines how a file is opened.
enum class FileFlag {
  kRead,    // Opens file for read access.
  kWrite,   // Opens file for write access.
  kReset,   // Clears file after opening, only valid with kWrite.
  kCreate,  // Creates file if it does not exist, only valid with kWrite.
};
using FileFlags = Flags<FileFlag>;
inline constexpr FileFlags kReadFileFlags = {
    FileFlag::kRead,
};
inline constexpr FileFlags kWriteFileFlags = {
    FileFlag::kWrite,
};
inline constexpr FileFlags kReadWriteFileFlags = {
    FileFlag::kRead,
    FileFlag::kWrite,
};
inline constexpr FileFlags kOverwriteFileFlags = {
    FileFlag::kWrite,
    FileFlag::kReset,
};
inline constexpr FileFlags kNewFileFlags = {
    FileFlag::kWrite,
    FileFlag::kCreate,
    FileFlag::kReset,
};

inline FileFlags FromFopenMode(std::string_view mode) {
  if (mode == "r" || mode == "rb" || mode == "rt") return kReadFileFlags;
  if (mode == "w" || mode == "wb" || mode == "wt") return kNewFileFlags;
  if (mode == "a" || mode == "ab" || mode == "at")
    return kWriteFileFlags + FileFlag::kCreate;
  if (mode == "r+" || mode == "r+b" || mode == "r+t")
    return kReadWriteFileFlags;
  if (mode == "w+" || mode == "w+b" || mode == "w+t")
    return kNewFileFlags + FileFlag::kRead;
  if (mode == "a+" || mode == "a+b" || mode == "a+t")
    return kReadWriteFileFlags + FileFlag::kCreate;
  return {};
}

enum class PathType {
  kInvalid,
  kFile,
  kFolder,
};
using PathTypes = Flags<PathType>;
inline constexpr PathTypes kAllPathTypes = {
    PathType::kFile,
    PathType::kFolder,
};

struct PathInfo {
  PathInfo() = default;
  PathInfo(PathType type, int64_t size = 0) : type(type), size(size) {}

  PathType type = PathType::kInvalid;  // Type of path.
  int64_t size = 0;                    // Set only for PathType::kFile.
};

}  // namespace gb

#endif  // GB_BASE_FILE_FILE_TYPES_H_
