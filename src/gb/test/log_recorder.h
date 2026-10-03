// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_TEST_LOG_RECORDER_H_
#define GB_TEST_LOG_RECORDER_H_

#include <string>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/base/thread_annotations.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"

namespace gb {

// Records Abseil log messages, so a test can check what was logged.
//
// For as long as it exists, a LogRecorder records every message at or above
// its severity, logged on any thread. A test takes them and checks them with
// matchers:
//
//   LogRecorder log_recorder;
//   DoWork();
//   EXPECT_THAT(log_recorder.TakeMessages(),
//               ElementsAre(Field(&LogRecorder::Message::text, "Started"),
//                           Field(&LogRecorder::Message::text, "Done")));
//
// Every LogRecorder that exists records each message, independently of the
// others.
//
// It leaves Abseil's global logging state (the minimum log level, the stderr
// threshold, and other sinks) as it is, and works whether or not
// absl::InitializeLog() has been called. Messages below the minimum log level
// are never logged, so they are never recorded either.
class LogRecorder final {
 public:
  // A recorded log message.
  struct Message {
    template <typename Sink>
    friend void AbslStringify(Sink& sink, const Message& message) {
      absl::Format(&sink, "%s:%d: %s: %s", message.file, message.line,
                   absl::LogSeverityName(message.severity), message.text);
    }

    absl::LogSeverity severity;
    std::string file;
    int line;
    std::string text;
  };

  explicit LogRecorder(
      absl::LogSeverityAtLeast min_severity = absl::LogSeverityAtLeast::kInfo);
  LogRecorder(const LogRecorder&) = delete;
  LogRecorder& operator=(const LogRecorder&) = delete;
  ~LogRecorder();

  // Returns the messages recorded so far, in the order they were logged, and
  // clears them.
  std::vector<Message> TakeMessages();

 private:
  class Sink final : public absl::LogSink {
   public:
    // `recorder` must outlive this.
    explicit Sink(LogRecorder* recorder) : recorder_(recorder) {}

    void Send(const absl::LogEntry& entry) override;

   private:
    LogRecorder* const recorder_;
  };

  const absl::LogSeverityAtLeast min_severity_;
  absl::Mutex mutex_;
  std::vector<Message> messages_ ABSL_GUARDED_BY(mutex_);
  Sink sink_{this};
};

}  // namespace gb

#endif  // GB_TEST_LOG_RECORDER_H_
