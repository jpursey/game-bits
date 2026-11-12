// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/memory_file_protocol.h"

#include <atomic>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "gb/base/weak_ptr.h"
#include "gb/file/raw_file.h"

namespace gb {

struct MemoryFileProtocol::Node final : WeakScope<MemoryFileProtocol::Node> {
  Node(PathType type)
      : WeakScope<Node>(this), type(type), open(false), size(0) {}
  ~Node() { InvalidateWeakPtrs(); }

  const PathType type;
  std::atomic<bool> open;
  std::atomic<int64_t> size;
  std::vector<uint8_t> contents;

  // Read and written only under the protocol's lock.
  FileAttributes attributes;
};

class MemoryFileProtocol::MemoryFile : public RawFile {
 public:
  explicit MemoryFile(Node* node);
  ~MemoryFile() override;

  // Public overrides for RawFile.
  int64_t SeekEnd() override;
  int64_t SeekTo(int64_t position) override;
  int64_t Write(const void* buffer, int64_t size) override;
  bool Flush() override;
  int64_t Read(void* buffer, int64_t size) override;

 private:
  WeakPtr<Node> node_;
  int64_t position_ = 0;
};

MemoryFileProtocol::MemoryFile::MemoryFile(Node* node) : node_(node) {
  node->open = true;
}

MemoryFileProtocol::MemoryFile::~MemoryFile() {
  WeakLock<Node> lock(&node_);
  Node* node = lock.Get();
  if (node != nullptr) {
    node->open = false;
  }
}

int64_t MemoryFileProtocol::MemoryFile::SeekEnd() {
  WeakLock<Node> lock(&node_);
  Node* node = lock.Get();
  if (node == nullptr) {
    position_ = -1;
  } else {
    position_ = static_cast<int64_t>(node->contents.size());
  }
  return position_;
}

int64_t MemoryFileProtocol::MemoryFile::SeekTo(int64_t position) {
  WeakLock<Node> lock(&node_);
  Node* node = lock.Get();
  if (node == nullptr) {
    position_ = -1;
  } else {
    position_ =
        std::clamp(position, 0LL, static_cast<int64_t>(node->contents.size()));
  }
  return position_;
}

int64_t MemoryFileProtocol::MemoryFile::Write(const void* buffer,
                                              int64_t size) {
  WeakLock<Node> lock(&node_);
  Node* node = lock.Get();
  if (node == nullptr) {
    position_ = -1;
    return 0;
  }
  if (position_ + size > static_cast<int64_t>(node->contents.size())) {
    node->contents.resize(position_ + size);
    node->size = position_ + size;
  }
  std::memcpy(node->contents.data() + position_, buffer, size);
  position_ += size;
  return size;
}

// Writes go straight to the node, so this only checks that it still exists.
bool MemoryFileProtocol::MemoryFile::Flush() {
  WeakLock<Node> lock(&node_);
  if (lock.Get() == nullptr) {
    position_ = -1;
    return false;
  }
  return true;
}

int64_t MemoryFileProtocol::MemoryFile::Read(void* buffer, int64_t size) {
  WeakLock<Node> lock(&node_);
  Node* node = lock.Get();
  if (node == nullptr) {
    position_ = -1;
    return -1;
  }
  if (position_ + size > static_cast<int64_t>(node->contents.size())) {
    size = static_cast<int64_t>(node->contents.size()) - position_;
  }
  std::memcpy(buffer, node->contents.data() + position_, size);
  position_ += size;
  return size;
}

MemoryFileProtocol::MemoryFileProtocol(FileProtocolFlags flags,
                                       ReadOnlyFlags read_only_flags)
    : flags_(flags), read_only_flags_(read_only_flags) {
  nodes_["/"] = std::make_unique<Node>(PathType::kFolder);
}

MemoryFileProtocol::~MemoryFileProtocol() {}

FileProtocolFlags MemoryFileProtocol::GetFlags() const { return flags_; }

FileAttributes MemoryFileProtocol::GetAttributeFlags() const {
  return {FileAttribute::kReadOnly, FileAttribute::kHidden};
}

std::vector<std::string> MemoryFileProtocol::GetDefaultNames() const {
  return {"mem"};
}

void MemoryFileProtocol::Lock(LockType type) ABSL_NO_THREAD_SAFETY_ANALYSIS {
  if (type == LockType::kQuery) {
    mutex_.ReaderLock();
  } else {
    mutex_.WriterLock();
  }
}

void MemoryFileProtocol::Unlock(LockType type) ABSL_NO_THREAD_SAFETY_ANALYSIS {
  if (type == LockType::kQuery) {
    mutex_.ReaderUnlock();
  } else {
    mutex_.WriterUnlock();
  }
}

PathInfo MemoryFileProtocol::DoGetPathInfo(std::string_view protocol_name,
                                           std::string_view path) {
  auto it = nodes_.find(path);
  if (it == nodes_.end()) {
    return {};
  }
  Node* node = it->second.get();
  if (node->type == PathType::kFolder) {
    return {PathType::kFolder, 0, node->attributes};
  }
  return {PathType::kFile, node->size, node->attributes};
}

bool MemoryFileProtocol::BasicSetAttributes(std::string_view protocol_name,
                                            std::string_view path,
                                            FileAttributes attributes) {
  nodes_.find(path)->second->attributes = attributes;
  return true;
}

std::vector<std::string> MemoryFileProtocol::BasicList(
    std::string_view protocol_name, std::string_view path) {
  std::string prefix;
  if (path == "/") {
    prefix = "/";
  } else {
    prefix = absl::StrCat(path, "/");
  }
  std::vector<std::string> paths;
  for (auto it = nodes_.lower_bound(prefix);
       it != nodes_.end() && absl::StartsWith(it->first, prefix); ++it) {
    std::string_view item_path = it->first;
    item_path.remove_prefix(prefix.size());
    if (item_path.empty() ||
        item_path.find_first_of('/') != std::string_view::npos) {
      continue;
    }
    paths.emplace_back(absl::StrCat(protocol_name, ":", prefix, item_path));
  }
  return paths;
}

bool MemoryFileProtocol::BasicCreateFolder(std::string_view protocol_name,
                                           std::string_view path) {
  nodes_[absl::StrCat(path)] = std::make_unique<Node>(PathType::kFolder);
  return true;
}

bool MemoryFileProtocol::BasicDeleteFolder(std::string_view protocol_name,
                                           std::string_view path) {
  nodes_.erase(absl::StrCat(path));
  return true;
}

bool MemoryFileProtocol::BasicDeleteFile(std::string_view protocol_name,
                                         std::string_view path) {
  auto it = nodes_.find(path);
  Node* node = it->second.get();
  if (node->open) {
    return false;
  }
  if (read_only_flags_.IsSet(ReadOnlyFlag::kPreventDelete) &&
      node->attributes.IsSet(FileAttribute::kReadOnly)) {
    return false;
  }
  nodes_.erase(it);
  return true;
}

bool MemoryFileProtocol::BasicMoveFolder(std::string_view from_protocol_name,
                                         std::string_view from_path,
                                         std::string_view to_protocol_name,
                                         std::string_view to_path) {
  // Nothing is moved unless everything can be, so the folder's paths are all
  // found before any of them change.
  const std::string prefix = absl::StrCat(from_path, "/");
  std::vector<Nodes::iterator> moved = {nodes_.find(from_path)};
  for (auto it = nodes_.lower_bound(prefix);
       it != nodes_.end() && absl::StartsWith(it->first, prefix); ++it) {
    if (it->second->open) {
      return false;
    }
    moved.push_back(it);
  }
  for (const auto& it : moved) {
    auto node = nodes_.extract(it);
    node.key() = absl::StrCat(
        to_path, std::string_view(node.key()).substr(from_path.size()));
    nodes_.insert(std::move(node));
  }
  return true;
}

bool MemoryFileProtocol::BasicMoveFile(std::string_view from_protocol_name,
                                       std::string_view from_path,
                                       std::string_view to_protocol_name,
                                       std::string_view to_path) {
  auto it = nodes_.find(from_path);
  if (it->second->open) {
    return false;
  }
  auto node = nodes_.extract(it);
  node.key() = std::string(to_path);
  nodes_.insert(std::move(node));
  return true;
}

std::unique_ptr<RawFile> MemoryFileProtocol::BasicOpenFile(
    std::string_view protocol_name, std::string_view path, FileFlags flags) {
  if (flags.IsSet(FileFlag::kCreate)) {
    auto& new_node = nodes_[absl::StrCat(path)];
    new_node = std::make_unique<Node>(PathType::kFile);
    return std::make_unique<MemoryFile>(new_node.get());
  }

  Node* node = nodes_.find(path)->second.get();
  if (node->open) {
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kWrite) &&
      node->attributes.IsSet(FileAttribute::kReadOnly)) {
    return nullptr;
  }
  if (flags.IsSet(FileFlag::kReset)) {
    node->contents.clear();
    node->size = 0;
  }
  return std::make_unique<MemoryFile>(node);
}

}  // namespace gb
