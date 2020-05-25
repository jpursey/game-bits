#include "gbits/file/memory_file_protocol.h"

#include <atomic>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "gbits/base/weak_ptr.h"
#include "gbits/file/raw_file.h"

namespace gb {

struct MemoryFileProtocol::Node final : WeakScope<MemoryFileProtocol::Node> {
  Node(PathType type)
      : WeakScope<Node>(this), type(type), open(false), size(0) {}
  ~Node() { InvalidateWeakPtrs(); }

  const PathType type;
  std::atomic<bool> open;
  std::atomic<int64_t> size;
  std::vector<uint8_t> contents;
};

class MemoryFileProtocol::MemoryFile : public RawFile {
 public:
  explicit MemoryFile(Node* node);
  ~MemoryFile() override;

  // Public overrides for RawFile.
  int64_t SeekEnd() override;
  int64_t SeekTo(int64_t position) override;
  int64_t Write(const void* buffer, int64_t size) override;
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

int64_t MemoryFileProtocol::MemoryFile::Read(void* buffer, int64_t size) {
  WeakLock<Node> lock(&node_);
  Node* node = lock.Get();
  if (node == nullptr) {
    position_ = -1;
    return 0;
  }
  if (position_ + size > static_cast<int64_t>(node->contents.size())) {
    size = static_cast<int64_t>(node->contents.size()) - position_;
  }
  std::memcpy(buffer, node->contents.data() + position_, size);
  position_ += size;
  return size;
}

MemoryFileProtocol::MemoryFileProtocol(FileProtocolFlags flags)
    : flags_(flags) {
  nodes_["/"] = std::make_unique<Node>(PathType::kFolder);
}

MemoryFileProtocol::~MemoryFileProtocol() {}

FileProtocolFlags MemoryFileProtocol::GetFlags() const { return flags_; }

std::vector<std::string> MemoryFileProtocol::GetDefaultNames() const {
  return {"mem"};
}

void MemoryFileProtocol::Lock(LockType type) {
  if (type == LockType::kQuery) {
    mutex_.ReaderLock();
  } else {
    mutex_.WriterLock();
  }
}

void MemoryFileProtocol::Unlock(LockType type) {
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
    return {PathType::kFolder};
  }
  return {PathType::kFile, node->size};
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
  if (it->second->open) {
    return false;
  }
  nodes_.erase(it);
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
  if (flags.IsSet(FileFlag::kReset)) {
    node->contents.clear();
  }
  return std::make_unique<MemoryFile>(node);
}

}  // namespace gb
