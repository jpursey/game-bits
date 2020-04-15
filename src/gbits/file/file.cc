#include "gbits/file/file.h"

#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "gbits/file/raw_file.h"

namespace gb {

File::File(std::unique_ptr<RawFile> file, FileFlags flags)
    : file_(std::move(file)), flags_(flags) {}

File::~File() {}

int64_t File::SeekEnd() {
  position_ = file_->SeekEnd();
  return position_;
}

int64_t File::SeekTo(int64_t position) {
  position_ = file_->SeekTo(position);
  return position_;
}

bool File::ReadRemaining(std::string* buffer) {
  if (!flags_.IsSet(FileFlag::kRead)) {
    buffer->resize(0);
    return false;
  }
  const int64_t remaining = CalculateRemaining();
  if (remaining < 0) {
    buffer->resize(0);
    return false;
  }
  buffer->resize(static_cast<std::string::size_type>(remaining));
  if (remaining == 0) {
    return true;
  }
  const int64_t bytes_read = DoRead(&(*buffer)[0], remaining);
  buffer->resize(bytes_read);
  return bytes_read == remaining;
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
  ReadRemaining(&text);
  if (text.empty()) {
    lines->clear();
    return 0;
  }
  text = absl::StrReplaceAll(text, {{"\r\n", "\n"}, {"\r", "\n"}});
  *lines = absl::StrSplit(text, "\n");
  if (text.back() == '\n') {
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
  position_ += actual_size;
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

    line->append(state->buffer.c_str(), state->pos, pos - state->pos);
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
