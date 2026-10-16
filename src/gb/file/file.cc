// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/file.h"

#include <string_view>

#include "absl/log/log.h"
#include "absl/strings/str_split.h"
#include "gb/file/raw_file.h"

namespace gb {

namespace {

// An absl::StrSplit delimiter for any line ending: "\r\n", "\n", or "\r".
class ByLineEnding {
 public:
  // Abseil's delimiter interface, which takes a size_t position.
  std::string_view Find(std::string_view text, size_t pos) const {
    const size_t found = text.find_first_of("\r\n", pos);
    if (found == std::string_view::npos) {
      return text.substr(text.size());
    }
    if (text[found] == '\r' && found + 1 < text.size() &&
        text[found + 1] == '\n') {
      return text.substr(found, 2);
    }
    return text.substr(found, 1);
  }
};

}  // namespace

File::File(std::unique_ptr<RawFile> file, FileFlags flags)
    : file_(std::move(file)), flags_(flags) {}

File::~File() {
  if (IsValid() && !Flush()) {
    LOG(ERROR) << "What was written to a file failed to reach it when the file "
                  "was destroyed. Call File::Close() to check for this.";
  }
}

bool File::Flush() {
  if (position_ < 0) {
    return false;
  }
  if (flags_.IsSet(FileFlag::kWrite) && !file_->Flush()) {
    position_ = -1;
    return false;
  }
  return true;
}

bool File::Close() {
  const bool flushed = Flush();
  file_.reset();
  position_ = -1;
  return flushed;
}

int64_t File::SeekEnd() {
  if (position_ < 0) {
    return -1;
  }
  position_ = file_->SeekEnd();
  return position_;
}

int64_t File::SeekTo(int64_t position) {
  if (position_ < 0) {
    return -1;
  }
  position_ = file_->SeekTo(position);
  return position_;
}

void File::ReadRemainingString(std::string* buffer) {
  if (!flags_.IsSet(FileFlag::kRead)) {
    buffer->resize(0);
    return;
  }
  const int64_t remaining = CalculateRemaining();
  if (remaining < 0) {
    buffer->resize(0);
    return;
  }
  buffer->resize(static_cast<std::string::size_type>(remaining));
  if (remaining == 0) {
    return;
  }
  const int64_t bytes_read = DoRead(&(*buffer)[0], remaining);
  buffer->resize(bytes_read);
}

bool File::ReadLine(std::string* line) {
  ReadLineState state;
  if (!DoReadLine(&state, line)) {
    line->clear();
    return false;
  }
  if (state.pos < state.buffer.size()) {
    SeekBy(-static_cast<int64_t>(state.buffer.size() - state.pos));
  }
  return true;
}

int64_t File::ReadLines(int64_t count, std::vector<std::string>* lines) {
  lines->clear();
  lines->reserve(count);
  ReadLineState state;
  for (; count > 0; --count) {
    std::string line;
    if (!DoReadLine(&state, &line)) {
      return static_cast<int64_t>(lines->size());
    }
    lines->emplace_back(std::move(line));
  }
  if (state.pos < state.buffer.size()) {
    SeekBy(-static_cast<int64_t>(state.buffer.size() - state.pos));
  }
  return static_cast<int64_t>(lines->size());
}

int64_t File::ReadRemainingLines(std::vector<std::string>* lines) {
  std::string text;
  ReadRemainingString(&text);
  if (text.empty()) {
    lines->clear();
    return 0;
  }
  *lines = absl::StrSplit(text, ByLineEnding());

  // A line ending at the end of the text ends the last line, rather than
  // starting an empty one.
  if (text.back() == '\n' || text.back() == '\r') {
    lines->pop_back();
  }
  return static_cast<int64_t>(lines->size());
}

bool File::WriteLine(std::string_view line, std::string_view line_end) {
  if (position_ < 0) {
    return false;
  }
  const int64_t line_size = static_cast<int64_t>(line.size());
  if (line_size > 0 && DoWrite(line.data(), line_size) < line_size) {
    return false;
  }
  const int64_t line_end_size = static_cast<int64_t>(line_end.size());
  if (line_end_size > 0 &&
      DoWrite(line_end.data(), line_end_size) < line_end_size) {
    return false;
  }
  return true;
}

int64_t File::CalculateRemaining() {
  if (position_ < 0) {
    return -1;
  }
  int64_t end = file_->SeekEnd();
  if (end < 0 || file_->SeekTo(position_) < 0) {
    position_ = -1;
    return -1;
  }
  return end - position_;
}

int64_t File::DoWrite(const void* buffer, int64_t size) {
  if (position_ < 0 || !flags_.IsSet(FileFlag::kWrite)) {
    return 0;
  }
  const int64_t actual_size = file_->Write(buffer, size);

  // Unlike a short read, which reached the end of the file, a short write is
  // always an error.
  if (actual_size < size) {
    position_ = -1;
  } else {
    position_ += actual_size;
  }
  return actual_size;
}

int64_t File::DoRead(void* buffer, int64_t size) {
  if (position_ < 0 || !flags_.IsSet(FileFlag::kRead)) {
    return 0;
  }
  const int64_t actual_size = file_->Read(buffer, size);
  position_ += actual_size;
  return actual_size;
}

bool File::DoReadLine(ReadLineState* state, std::string* line) {
  line->clear();
  bool skip_linefeed = false;
  while (true) {
    if (state->pos >= state->buffer.size()) {
      state->buffer.resize(kLineBufferSize);
      int64_t read_bytes = DoRead(&state->buffer[0], kLineBufferSize);
      if (read_bytes > 0 && skip_linefeed && state->buffer[0] == '\n') {
        state->pos = 1;
      } else {
        state->pos = 0;
      }
      state->buffer.resize(static_cast<std::string::size_type>(read_bytes));
      if (skip_linefeed) {
        return true;
      }
      if (state->pos == state->buffer.size()) {
        return !line->empty();
      }
      skip_linefeed = false;
    }

    auto pos = state->buffer.find_first_of("\r\n", state->pos, 2);
    if (pos == std::string::npos) {
      line->append(state->buffer, state->pos);
      state->pos = state->buffer.size();
      continue;
    }

    line->append(state->buffer, state->pos, pos - state->pos);
    if (state->buffer[pos] == '\r') {
      if (state->buffer.size() == pos + 1) {
        skip_linefeed = true;
      } else if (state->buffer[pos + 1] == '\n') {
        ++pos;
      }
    }
    state->pos = pos + 1;
    if (!skip_linefeed) {
      break;
    }
  }
  return true;
}

}  // namespace gb
