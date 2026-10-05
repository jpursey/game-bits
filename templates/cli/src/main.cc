#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "gb/base/init_logging.h"

int main(int argc, char* argv[]) {
  gb::InitLogging();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  LOG(INFO) << "Hello World";
}
