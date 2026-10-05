// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_LOG_FILE_H_
#define GB_BASE_LOG_FILE_H_

#include <filesystem>
#include <memory>

#include "absl/base/log_severity.h"
#include "absl/log/log_sink.h"

namespace gb {

// Writes Abseil log messages to a file for as long as it exists.
//
// Each message is a line starting with the severity, the local time, the
// thread ID, and the source file and line. Messages may be logged from any
// thread, and each one is flushed as it is written, so the file is complete
// even after a crash or a failed CHECK.
class LogFile final {
 public:
  // Starts writing log messages at or above `min_level` to the file at
  // `path`, replacing the file if it exists. If the file can't be created,
  // this logs an error and the LogFile writes nothing.
  //
  // Messages below Abseil's minimum log level are never logged, so they are
  // never written either, whatever `min_level` is. Other log sinks and stderr
  // are unaffected by `min_level`.
  LogFile(const std::filesystem::path& path,
          absl::LogSeverityAtLeast min_level);
  LogFile(const LogFile&) = delete;
  LogFile& operator=(const LogFile&) = delete;
  ~LogFile();

 private:
  std::unique_ptr<absl::LogSink> sink_;
};

}  // namespace gb

#endif  // GB_BASE_LOG_FILE_H_
