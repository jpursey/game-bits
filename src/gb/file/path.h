#ifndef GB_FILE_PATH_H_
#define GB_FILE_PATH_H_

#include <string>
#include <string_view>

#include "gb/base/flags.h"

namespace gb {

// Path flags determine what parts of a path are allowed or required in a given
// context.
enum class PathFlag {
  kRequireProtocol,     // Path must have a protocol.
  kAllowProtocol,       // Path may have a protocol.
  kRequireRoot,         // Path must be a root path (implied by kRequireHost).
  kRequireHost,         // Path must have a host (leading // followed by host).
  kAllowHost,           // Path may have a host (leading // followed by host).
  kAllowTrailingSlash,  // Non-root paths may also have a trailing slash.
  kRequireLowercase,    // Path must be lower case.
};
using PathFlags = Flags<PathFlag>;
inline constexpr PathFlags kLocalPathFlags = {};
inline constexpr PathFlags kUrlPathFlags = {PathFlag::kRequireProtocol,
                                            PathFlag::kRequireHost};
inline constexpr PathFlags kGenericPathFlags = {PathFlag::kAllowProtocol,
                                                PathFlag::kAllowHost};
inline constexpr PathFlags kProtocolPathFlags = {PathFlag::kRequireProtocol,
                                                 PathFlag::kAllowProtocol};
inline constexpr PathFlags kHostPathFlags = {PathFlag::kAllowHost,
                                             PathFlag::kRequireHost};

// Returns true if the protocol name is valid for use in a path.
//
// A valid protocol name is non-empty, containing lowercase ASCII alpha-numeric
// characters only.
bool IsValidProtocolName(std::string_view protocol_name);

// Removes any protocol name from the path, returning the new path and
// optionally storing the found protocol in 'protocol_name'.
//
// If specified, flags determine whether the protocol is considered in the path.
// If neither kAllowProtocol nor kRequireProtocol are present in the flags, this
// will always return an empty string. If no flags are specifed, then the
// protocol is considered (the same as passing PathFlag::kAllowProtocol).
//
// Input path is expected to be normalized (see NormalizePath).
std::string_view RemoveProtocol(std::string_view path, PathFlags flags,
                                std::string_view* protocol_name = nullptr);
inline std::string_view RemoveProtocol(
    const char* path, PathFlags flags,
    std::string_view* protocol_name = nullptr) {
  return RemoveProtocol(std::string_view(path), flags, protocol_name);
}
std::string RemoveProtocol(const std::string& path, PathFlags flags,
                           std::string* protocol_name = nullptr);
inline std::string_view RemoveProtocol(
    std::string_view path, std::string_view* protocol_name = nullptr) {
  return RemoveProtocol(path, PathFlag::kAllowProtocol, protocol_name);
}
inline std::string_view RemoveProtocol(
    const char* path, std::string_view* protocol_name = nullptr) {
  return RemoveProtocol(path, PathFlag::kAllowProtocol, protocol_name);
}
inline std::string RemoveProtocol(const std::string& path,
                                  std::string* protocol_name = nullptr) {
  return RemoveProtocol(path, PathFlag::kAllowProtocol, protocol_name);
}

// Removes any root from the path.
//
// If specified, flags determine whether protocol and/or host are considered in
// the path (if they are, they are always considered part of the root). If no
// flags are specified, then both the protocol and host are considered if they
// are present (the same as passing kGenericPathFlags).
//
// Returns the new path (relative to the root) and optionally storing the found
// root in 'root'. The stored root is terminated in a path separator only if it
// is a non-host root path.
//
// Input path is expected to be normalized (see NormalizePath).
std::string_view RemoveRoot(std::string_view path, PathFlags flags,
                            std::string_view* root = nullptr);
inline std::string_view RemoveRoot(const char* path, PathFlags flags,
                                   std::string_view* root = nullptr) {
  return RemoveRoot(std::string_view(path), flags, root);
}
std::string RemoveRoot(const std::string& path, PathFlags flags,
                       std::string* root = nullptr);
inline std::string_view RemoveRoot(std::string_view path,
                                   std::string_view* root = nullptr) {
  return RemoveRoot(path, kGenericPathFlags, root);
}
inline std::string_view RemoveRoot(const char* path,
                                   std::string_view* root = nullptr) {
  return RemoveRoot(path, kGenericPathFlags, root);
}
inline std::string RemoveRoot(const std::string& path,
                              std::string* root = nullptr) {
  return RemoveRoot(path, kGenericPathFlags, root);
}

// Retrieves the host name from the path, if there is one.
//
// If specified, flags determine whether protocol and/or host are considered in
// the path. If neither kAllowHost nor kRequireHost are present in the flags,
// this will always return an empty string. If no flags are specified, then both
// the protocol and host are considered if they are present (the same as passing
// kGenericPathFlags).
//
// Input path is expected to be normalized (see NormalizePath).
std::string_view GetHostName(std::string_view path,
                             PathFlags flags = kGenericPathFlags);
inline std::string_view GetHostName(const char* path,
                                    PathFlags flags = kGenericPathFlags) {
  return GetHostName(std::string_view(path), flags);
}
std::string GetHostName(const std::string& path,
                        PathFlags flags = kGenericPathFlags);

// Removes any filename from the path.
//
// If specified, flags determine whether protocol and/or host are considered in
// the path (if they are, they are always considered part of the folder). If no
// flags are specified, then both the protocol and host are considered if they
// are present (the same as passing kGenericPathFlags).
//
// Returns the new path and optionally storing the found filename in 'filename'.
// The resulting path is never terminated in a path separator, unless it is a
// non-host root path.
//
// Input path is expected to be normalized (see NormalizePath).
std::string_view RemoveFilename(std::string_view path, PathFlags flags,
                                std::string_view* filename = nullptr);
inline std::string_view RemoveFilename(const char* path, PathFlags flags,
                                       std::string_view* filename = nullptr) {
  return RemoveFilename(std::string_view(path), flags, filename);
}
std::string RemoveFilename(const std::string& path, PathFlags flags,
                           std::string* filename = nullptr);
inline std::string_view RemoveFilename(std::string_view path,
                                       std::string_view* filename = nullptr) {
  return RemoveFilename(path, kGenericPathFlags, filename);
}
inline std::string_view RemoveFilename(const char* path,
                                       std::string_view* filename = nullptr) {
  return RemoveFilename(path, kGenericPathFlags, filename);
}
inline std::string RemoveFilename(const std::string& path,
                                  std::string* filename = nullptr) {
  return RemoveFilename(path, kGenericPathFlags, filename);
}

// Removes any folder from the path.
//
// If specified, flags determine whether protocol and/or host are considered in
// the path (if they are, they are always considered part of the folder). If no
// flags are specified, then both the protocol and host are considered if they
// are present (the same as passing kGenericPathFlags).
//
// Returns the new path (just the filename) and optionally storing the found
// folder in 'folder'.  The stored folder is never terminated in a path
// separator, unless it is a non-host root path.
//
// Input path is expected to be normalized (see NormalizePath).
std::string_view RemoveFolder(std::string_view path, PathFlags flags,
                              std::string_view* folder = nullptr);
inline std::string_view RemoveFolder(const char* path, PathFlags flags,
                                     std::string_view* folder = nullptr) {
  return RemoveFolder(std::string_view(path), flags, folder);
}
std::string RemoveFolder(const std::string& path, PathFlags flags,
                         std::string* folder = nullptr);
inline std::string_view RemoveFolder(std::string_view path,
                                     std::string_view* folder = nullptr) {
  return RemoveFolder(path, kGenericPathFlags, folder);
}
inline std::string_view RemoveFolder(const char* path,
                                     std::string_view* folder = nullptr) {
  return RemoveFolder(path, kGenericPathFlags, folder);
}
inline std::string RemoveFolder(const std::string& path,
                                std::string* folder = nullptr) {
  return RemoveFolder(path, kGenericPathFlags, folder);
}

// Returns true if the path is an absolute path or false if it is relative.
//
// If flags contain kAllowProtocol or kRequireProtocol, then any protocol
// present in the path will be handled. If flags contain kAllowHost or
// kRequireHost, then paths that start with a host are considered absolute.
//
// Input path is expected to be normalized (see NormalizePath).
inline bool IsPathAbsolute(std::string_view path,
                           PathFlags flags = kGenericPathFlags) {
  path = RemoveProtocol(path, flags);
  return !path.empty() && path[0] == '/';
}

// Returns true if the path is an absolute path to the root folder.
//
// If flags contain kAllowProtocol or kRequireProtocol, then any protocol
// present in the path will be handled. If flags contain kAllowHost or
// kRequireHost, then a host on its own, with or without a trailing path
// separator is considered a root path.
//
// Input path is expected to be normalized (see NormalizePath).
inline bool IsRootPath(std::string_view path,
                       PathFlags flags = kGenericPathFlags) {
  return RemoveRoot(path, flags).empty();
}

// Appends path_b to path_a, separating with a path separator.
//
// If path flags contain protocol and or host flags, then path_a and path_b must
// having matching protocols and/or hosts (as specified by the flags). If a
// protocol and/or host exists in one path but not the other, this is considered
// a match, and the final path will use the non-empty protocol and/or host. If
// they do not match, then JoinPath will return an empty string.
//
// Input paths are expected to be normalized (see NormalizePath).
std::string JoinPath(std::string_view path_a, std::string_view path_b,
                     PathFlags flags = kGenericPathFlags);

// Returns true if the path matches the specified pattern.
//
// A pattern consists of direct characters to match, combined with '*' which
// will match zero or more characters. The pattern must match the whole string
// to be considered a match.
//
// This match is for the entirety of the path. If only the filename is desired
// to be matched, then pass in RemoveFolder(path).
//
// Matching is a pure text match, and so does not technically require
// normalization of the path, although for most practical purposes it is likely
// useful if it is.
bool PathMatchesPattern(std::string_view path, std::string_view pattern);

// Normalizes the path according to the specified flags.
//
// A generic normalized path is structured as follows. Optional parts of the
// path are denoted in square brackets. <protocol> is any string of lowercase
// ASCII alpha-numeric characters, <host> and <segment> are any characters that
// are not \ or /. The protocol and host are not always expected (or required)
// in all paths, as determined by flags.
//   [<protocol>:][//<host> or /[<segment>] or <segment>][/<segment>]...[/]
//
// Normalization always does the following:
// - Converts all \ path separators to /.
// - Collapses redundant path separators.
// - Collapses . path segments.
// - Collapses .. path segments with prior non-host segment, if there is one.
//
// In addition, normalization does the following based on the flags present:
// - kRequireProtocol: If a protocol is present, but not lowercase, it will be
//   made lowercase. Otherwise, fails if a protocol is not present or is
//   invalid. On failure, kRequireProtocol will be set as the failed flag.
// - kAllowProtocol: Ignored is kRequireProtocol is set. If a protocol is
//   present, but not lowercase, it will be made lowercase. Otherwise, fails if
//   a protocol is present and is invalid. On failure, kAllowProtocol will be
//   set as the failed flag.
// - Neither kRequireProtocol nor kAllowProtocol: No protocol normalization is
//   done, and the path is assumed to not have a protocol. Any protocol-like
//   prefix will be considered the first path segment.
// - kRequireRoot: The path must be a root path or specify a host (path after
//   protocol starts with a path separator). On failure, kRequireRoot will be
//   set as the failed flag.
// - kRequireHost: The path must start with "//" and be followed by a non-empty
//   path segment, or it will fail. The following path segment is considered a
//   host. On failure, kRequireHost will be set as the failed flag.
// - kAllowHost: Ignored if kRequiredHost is set. The path may start with "//"
//   and the following path segment is considered a host. If no path segment
//   follows the "//" prefix, this will fail and kAllowHost will be set as the
//   failed flag.
// - Neither kAllowHost nor kRequireHost: No host normalization is done, so any
//   leading "//" will be collapsed to a single "/", just like all other
//   redundant path separators.
// - kAllowTrailingSlash: The path may end with "/".
// - No kAllowTrailingSlash: If the path ends with any "/", they will be
//   stripped.
// - kRequireLowercase: Any uppercase ASCII characters will be made lowercase.
//   This has no effect on other UTF-8 code points.
//
// If the path cannot be normalized given the specified flags, this returns an
// empty string. If failed_flag is specified, then any flag that caused the
// normalization to fail will be set as described above.
std::string NormalizePath(std::string_view path,
                          PathFlags flags = kGenericPathFlags,
                          PathFlags* failed_flag = nullptr);
inline std::string NormalizePath(std::string_view path,
                                 PathFlags* failed_flag) {
  return NormalizePath(path, kGenericPathFlags, failed_flag);
}

}  // namespace gb

#endif  // GB_FILE_PATH_H_
