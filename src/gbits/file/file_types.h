#ifndef GBITS_BASE_FILE_FILE_TYPES_H_
#define GBITS_BASE_FILE_FILE_TYPES_H_

#include <stdint.h>

#include "gbits/base/flags.h"

namespace gb {

class File;
class FileProtocol;
class FileSystem;
class RawFile;

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
};
using FileProtocolFlags = Flags<FileProtocolFlag>;

// Protocol supporting all features.
inline constexpr FileProtocolFlags kAllFileProtocolFlags = {
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
inline constexpr FileFlags kReadWriteFileFlags = {
    FileFlag::kRead,
    FileFlag::kWrite,
};
inline constexpr FileFlags kNewFileFlags = {
    kReadWriteFileFlags,
    FileFlag::kCreate,
    FileFlag::kReset,
};

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

#endif  // GBITS_BASE_FILE_FILE_TYPES_H_
