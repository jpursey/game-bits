// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/log_file.h"

#ifdef _WIN32
#include <share.h>
#endif  // _WIN32

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

#include "absl/base/log_severity.h"
#include "absl/base/thread_annotations.h"
#include "absl/log/log.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink_registry.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "gb/base/local_time.h"
#include "gb/base/unicode.h"

namespace gb {
namespace {

// Creates the file at `path` for writing text, replacing it if it exists.
// Returns null if it can't be created.
std::FILE* CreateTextFile(const std::filesystem::path& path) {
#ifdef _WIN32
  // Unlike _wfopen_s(), this lets other programs read the file while it is
  // open.
  return _wfsopen(path.c_str(), L"w", _SH_DENYNO);
#else
  return std::fopen(path.c_str(), "w");
#endif
}

class FileSink final : public absl::LogSink {
 public:
  // Takes ownership of `file`, which must be open for writing.
  FileSink(std::FILE* file, absl::LogSeverityAtLeast min_level)
      : file_(file), min_level_(min_level), time_zone_(GetLocalTimeZone()) {}
  ~FileSink() override { std::fclose(file_); }

  void Send(const absl::LogEntry& entry) override {
    if (entry.log_severity() < min_level_) {
      return;
    }
    const std::string line = absl::StrFormat(
        "%c%s %d (%s:%d) %s\n", absl::LogSeverityName(entry.log_severity())[0],
        absl::FormatTime("%m%d %H:%M:%E6S", entry.timestamp(), time_zone_),
        entry.tid(), entry.source_basename(), entry.source_line(),
        entry.text_message());
    absl::MutexLock lock(&mutex_);
    std::fwrite(line.data(), 1, line.size(), file_);
    std::fflush(file_);
  }

 private:
  absl::Mutex mutex_;
  std::FILE* const file_ ABSL_PT_GUARDED_BY(mutex_);
  const absl::LogSeverityAtLeast min_level_;
  const absl::TimeZone time_zone_;
};

}  // namespace

LogFile::LogFile(const std::filesystem::path& path,
                 absl::LogSeverityAtLeast min_level) {
  std::FILE* file = CreateTextFile(path);
  if (file == nullptr) {
    LOG(ERROR) << "Failed to create log file: " << ToUtf8(path.u16string());
    return;
  }
  sink_ = std::make_unique<FileSink>(file, min_level);
  absl::AddLogSink(sink_.get());
}

LogFile::~LogFile() {
  if (sink_ != nullptr) {
    absl::RemoveLogSink(sink_.get());
  }
}

}  // namespace gb
