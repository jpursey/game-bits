// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/test/log_recorder.h"

#include <utility>

#include "absl/log/log_sink_registry.h"

namespace gb {

LogRecorder::LogRecorder(absl::LogSeverityAtLeast min_severity)
    : min_severity_(min_severity) {
  absl::AddLogSink(&sink_);
}

LogRecorder::~LogRecorder() {
  // Removing the sink waits for any message being sent to it on another
  // thread, so the sink is never used after this.
  absl::RemoveLogSink(&sink_);
}

std::vector<LogRecorder::Message> LogRecorder::TakeMessages() {
  absl::MutexLock lock(&mutex_);
  return std::exchange(messages_, {});
}

void LogRecorder::Sink::Send(const absl::LogEntry& entry) {
  if (entry.log_severity() < recorder_->min_severity_) {
    return;
  }
  absl::MutexLock lock(&recorder_->mutex_);
  recorder_->messages_.push_back({.severity = entry.log_severity(),
                                  .file = std::string(entry.source_filename()),
                                  .line = entry.source_line(),
                                  .text = std::string(entry.text_message())});
}

}  // namespace gb
