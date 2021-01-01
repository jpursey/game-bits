#include "glog/logging.h"

int main(int argc, char* argv[]) {
  FLAGS_alsologtostderr = 1;
  google::InitGoogleLogging(argv[0]);
  google::EnableLogCleaner(1);

  LOG(INFO) << "Hello World";
}
