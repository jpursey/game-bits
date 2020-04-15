#include "gbits/file/path.h"

#include "absl/container/inlined_vector.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"

namespace gb {

bool IsValidProtocolName(std::string_view protocol_name) {
  if (protocol_name.empty()) {
    return false;
  }
  for (auto ch : protocol_name) {
    if (!absl::ascii_islower(ch) && !absl::ascii_isdigit(ch)) {
      return false;
    }
  }
  return true;
}

std::string_view RemoveProtocol(std::string_view path, PathFlags flags,
                                std::string_view* protocol_name) {
  if (!flags.Intersects(kProtocolPathFlags)) {
    if (protocol_name != nullptr) {
      *protocol_name = {};
    }
    return path;
  }
  auto pos = path.find(':');
  if (pos != std::string_view::npos &&
      IsValidProtocolName(path.substr(0, pos))) {
    if (protocol_name != nullptr) {
      *protocol_name = path.substr(0, pos);
    }
    path.remove_prefix(pos + 1);
    return path;
  }

  if (protocol_name != nullptr) {
    *protocol_name = {};
  }
  return path;
}
std::string RemoveProtocol(const std::string& path, PathFlags flags,
                           std::string* protocol_name) {
  std::string_view path_view(path.data(), path.size());
  std::string_view result;
  if (protocol_name == nullptr) {
    result = RemoveProtocol(path_view, flags);
  } else {
    std::string_view protocol_name_view;
    result = RemoveProtocol(path_view, flags, &protocol_name_view);
    protocol_name->assign(protocol_name_view.data(), protocol_name_view.size());
  }
  return std::string(result.data(), result.size());
}

std::string_view RemoveRoot(std::string_view path, PathFlags flags,
                            std::string_view* root) {
  auto stripped_path = RemoveProtocol(path, flags);
  auto protocol_prefix_size = path.size() - stripped_path.size();
  if (flags.Intersects(kHostPathFlags) &&
      absl::StartsWith(stripped_path, "//")) {
    auto pos = stripped_path.find('/', 2);
    if (pos == std::string_view::npos) {
      stripped_path = {};
    } else {
      stripped_path = stripped_path.substr(pos + 1);
    }
  } else if (!stripped_path.empty() && stripped_path.front() == '/') {
    auto pos = stripped_path.find_first_not_of("/");
    if (pos == std::string_view::npos) {
      pos = stripped_path.size();
    }
    stripped_path.remove_prefix(pos);
  }
  if (root != nullptr) {
    *root = path.substr(0, path.size() - stripped_path.size());
    if (root->size() > protocol_prefix_size + 1 && root->back() == '/') {
      root->remove_suffix(1);
    }
  }
  return stripped_path;
}
std::string RemoveRoot(const std::string& path, PathFlags flags,
                       std::string* root) {
  std::string_view path_view(path.data(), path.size());
  std::string_view result;
  if (root == nullptr) {
    result = RemoveRoot(path_view, flags);
  } else {
    std::string_view root_view;
    result = RemoveRoot(path_view, flags, &root_view);
    root->assign(root_view.data(), root_view.size());
  }
  return std::string(result.data(), result.size());
}

std::string_view GetHostName(std::string_view path, PathFlags flags) {
  if (!flags.Intersects(kHostPathFlags)) {
    return {};
  }
  path = RemoveProtocol(path, flags);
  if (!absl::StartsWith(path, "//")) {
    return {};
  }
  path.remove_prefix(2);
  auto pos = path.find('/');
  if (pos == std::string_view::npos) {
    return path;
  }
  return path.substr(0, pos);
}
std::string GetHostName(const std::string& path, PathFlags flags) {
  return absl::StrCat(GetHostName(std::string_view(path), flags));
}

std::string_view RemoveFilename(std::string_view path, PathFlags flags,
                                std::string_view* filename) {
  if (path.empty()) {
    if (filename != nullptr) {
      *filename = {};
    }
    return {};
  }

  std::string_view root;
  std::string_view local_path = RemoveRoot(path, flags, &root);
  if (local_path.empty()) {
    if (filename != nullptr) {
      *filename = {};
    }
    return root;
  }

  auto pos = local_path.rfind('/');
  if (pos != std::string_view::npos) {
    if (filename != nullptr) {
      *filename = local_path.substr(pos + 1);
    }
    path.remove_suffix(local_path.size() - pos);
  } else {
    if (filename != nullptr) {
      *filename = local_path;
    }
    path.remove_suffix(local_path.size());
    if (path.size() > 1 && path.back() == '/' &&
        path.find_last_of('/', path.size() - 2) != std::string_view::npos) {
      path.remove_suffix(1);
    }
  }
  return path;
}
std::string RemoveFilename(const std::string& path, PathFlags flags,
                           std::string* filename) {
  std::string_view path_view(path.data(), path.size());
  std::string_view result;
  if (filename == nullptr) {
    result = RemoveFilename(path_view, flags);
  } else {
    std::string_view filename_view;
    result = RemoveFilename(path_view, flags, &filename_view);
    filename->assign(filename_view.data(), filename_view.size());
  }
  return std::string(result.data(), result.size());
}

std::string_view RemoveFolder(std::string_view path, PathFlags flags,
                              std::string_view* folder) {
  std::string_view filename;
  std::string_view folder_path = RemoveFilename(path, flags, &filename);
  if (folder != nullptr) {
    *folder = folder_path;
  }
  return filename;
}
std::string RemoveFolder(const std::string& path, PathFlags flags,
                         std::string* folder) {
  std::string_view path_view(path.data(), path.size());
  std::string_view result;
  if (folder == nullptr) {
    result = RemoveFolder(path_view, flags);
  } else {
    std::string_view folder_view;
    result = RemoveFolder(path_view, flags, &folder_view);
    folder->assign(folder_view.data(), folder_view.size());
  }
  return std::string(result.data(), result.size());
}

std::string JoinPath(std::string_view path_a, std::string_view path_b,
                     PathFlags flags) {
  std::string result;
  result.reserve(path_a.size() + path_b.size() + 1);

  if (flags.Intersects(kProtocolPathFlags)) {
    std::string_view protocol_a;
    path_a = RemoveProtocol(path_a, flags, &protocol_a);

    std::string_view protocol_b;
    path_b = RemoveProtocol(path_b, flags, &protocol_b);

    if (!protocol_a.empty()) {
      if (!protocol_b.empty() && protocol_a != protocol_b) {
        return {};
      }
      result.append(protocol_a.data(), protocol_a.size());
      result.append(1, ':');
    } else if (!protocol_b.empty()) {
      result.append(protocol_b.data(), protocol_b.size());
      result.append(1, ':');
    }
  }

  if (flags.Intersects(kHostPathFlags)) {
    std::string_view host_a = GetHostName(path_a, flags);
    std::string_view host_b = GetHostName(path_b, flags);
    if (!host_a.empty()) {
      if (!host_b.empty()) {
        if (host_a != host_b) {
          return {};
        }
        path_b.remove_prefix(host_b.size() + 2);
      }
      path_a.remove_prefix(host_a.size() + 2);
      result.append("//", 2);
      result.append(host_a.data(), host_a.size());
      if (path_a.empty() && !path_b.empty() && path_b[0] != '/') {
        result.append(1, '/');
      }
    } else if (!host_b.empty()) {
      path_b.remove_prefix(host_b.size() + 2);
      result.append("//", 2);
      result.append(host_b.data(), host_b.size());
      if ((!path_b.empty() && path_b[0] == '/') ||
          (path_b.empty() && !path_a.empty() && path_a[0] != '/')) {
        result.append(1, '/');
      }
    }
  }

  if (path_a.empty()) {
    result.append(path_b.data(), path_b.size());
    return result;
  }
  if (!path_b.empty() && path_b[0] == '/') {
    path_b.remove_prefix(1);
  }
  if (path_b.empty()) {
    result.append(path_a.data(), path_a.size());
    return result;
  }
  result.append(path_a.data(), path_a.size());
  if (path_a[path_a.size() - 1] != '/') {
    result.append(1, '/');
  }
  result.append(path_b.data(), path_b.size());
  return result;
}

bool PathMatchesPattern(std::string_view path, std::string_view pattern) {
  std::string_view::size_type path_pos = 0;
  std::string_view::size_type pattern_pos = 0;

  while (path_pos < path.size() && pattern_pos < pattern.size() &&
         pattern[pattern_pos] != '*') {
    if (path[path_pos++] != pattern[pattern_pos++]) {
      return false;
    }
  }
  if (pattern_pos == pattern.size()) {
    return path_pos == path.size();
  } else if (pattern[pattern_pos] != '*') {
    return false;
  }

  while (pattern_pos < pattern.size()) {
    ++pattern_pos;
    std::string_view::size_type subpattern_end = pattern_pos;
    while (subpattern_end < pattern.size() && pattern[subpattern_end] != '*') {
      ++subpattern_end;
    }
    std::string_view subpattern(pattern.data() + pattern_pos,
                                subpattern_end - pattern_pos);
    pattern_pos = subpattern_end;
    if (subpattern.empty()) {
      if (pattern_pos == pattern.size()) {
        return true;
      }
      continue;
    }

    auto find_pos = path.rfind(subpattern);
    if (find_pos == std::string_view::npos || find_pos < path_pos) {
      return false;
    }
    path_pos = find_pos + subpattern.size();
  }
  return path_pos == path.size();
}

namespace {

// Inline utility functions for NormalizePath.

inline bool IsSeparator(const char* in, const char* in_end) {
  return in < in_end && (*in == '\\' || *in == '/');
}

inline bool IsNonSeparator(const char* in, const char* in_end) {
  return in < in_end && *in != '\\' && *in != '/';
}

}  // namespace

std::string NormalizePath(std::string_view path, PathFlags flags,
                          PathFlags* failed_flag) {
  std::string new_path(path.size(), 0);
  char* out = &new_path[0];
  const char* in = path.data();
  const char* in_end = path.data() + path.size();
  absl::InlinedVector<char*, 16> segments;

  // Check for a protocol first, if requested.
  std::string_view::size_type protocol_size = 0;
  if (flags.Intersects(kProtocolPathFlags)) {
    if (in != in_end && *in == ':') {
      if (failed_flag != nullptr) {
        if (flags.IsSet(PathFlag::kRequireProtocol)) {
          *failed_flag = PathFlag::kRequireProtocol;
        } else {
          *failed_flag = PathFlag::kAllowProtocol;
        }
      }
      return {};
    }
    const char* protocol_end = in;
    while (protocol_end != in_end && absl::ascii_isalnum(*protocol_end)) {
      ++protocol_end;
    }
    if (protocol_end != in_end && *protocol_end == ':') {
      while (in != protocol_end) {
        *out++ = absl::ascii_tolower(*in++);
      }
      *out++ = *in++;
      protocol_size = out - new_path.data();
    } else if (flags.IsSet(PathFlag::kRequireProtocol)) {
      if (failed_flag != nullptr) {
        *failed_flag = PathFlag::kRequireProtocol;
      }
      return {};
    } else {
      while (IsNonSeparator(protocol_end, in_end)) {
        if (*protocol_end == ':') {
          if (failed_flag != nullptr) {
            *failed_flag = PathFlag::kAllowProtocol;
          }
          return {};
        }
        ++protocol_end;
      }
    }
  }

  // Validate root.
  bool segment_is_host = false;
  if (!IsSeparator(in, in_end)) {
    if (flags.IsSet(PathFlag::kRequireHost)) {
      if (failed_flag != nullptr) {
        *failed_flag = PathFlag::kRequireHost;
      }
      return {};
    }
    if (flags.IsSet(PathFlag::kRequireRoot)) {
      if (failed_flag != nullptr) {
        *failed_flag = PathFlag::kRequireRoot;
      }
      return {};
    }
  } else if (flags.Intersects(kHostPathFlags)) {
    in = ++in;
    *out++ = '/';
    segment_is_host = IsSeparator(in, in_end);
    if (flags.IsSet(PathFlag::kRequireHost) && !segment_is_host) {
      if (failed_flag != nullptr) {
        *failed_flag = PathFlag::kRequireHost;
      }
      return {};
    }
  }

  // Process the path.
  while (in < in_end) {
    // Consume extra path separators.
    if (IsSeparator(in, in_end)) {
      *out++ = '/';
      ++in;
      while (IsSeparator(in, in_end)) {
        ++in;
      }
    }

    if (segment_is_host) {
      segment_is_host = false;
      if (in == in_end) {
        if (failed_flag != nullptr) {
          if (flags.IsSet(PathFlag::kRequireHost)) {
            *failed_flag = PathFlag::kRequireHost;
          } else {
            *failed_flag = PathFlag::kAllowHost;
          }
        }
        return {};
      }
    } else {
      // Collapse dot sequences.
      bool is_dot_path = false;
      while (in != in_end && *in == '.') {
        // ./
        if (IsSeparator(in + 1, in_end) || in + 1 == in_end) {
          in = in + 2;
          while (IsSeparator(in, in_end)) {
            ++in;
          }
          continue;
        }
        // ../
        if ((IsSeparator(in + 2, in_end) || in + 2 == in_end) && in[1] == '.') {
          if (segments.empty()) {
            is_dot_path = true;
            break;
          }
          out = segments.back();
          segments.pop_back();
          in = in + 3;
          while (IsSeparator(in, in_end)) {
            ++in;
          }
        }
      }
      if (!is_dot_path) {
        segments.push_back(out);
      } else if (flags.IsSet(PathFlag::kRequireRoot)) {
        if (failed_flag != nullptr) {
          *failed_flag = PathFlag::kRequireRoot;
        }
        return {};
      }
    }

    // Append segment.
    while (IsNonSeparator(in, in_end)) {
      if (flags.IsSet(PathFlag::kRequireLowercase) &&
          absl::ascii_isalpha(*in)) {
        *out++ = absl::ascii_tolower(*in++);
      } else {
        *out++ = *in++;
      }
    }
  }

  new_path.resize(out - new_path.data());

  // Trim trailing slash.
  if (new_path.size() > protocol_size + 1 && new_path.back() == '/' &&
      !flags.IsSet(PathFlag::kAllowTrailingSlash)) {
    new_path.pop_back();
  }

  if (failed_flag != nullptr) {
    *failed_flag = {};
  }
  return new_path;
}

}  // namespace gb
