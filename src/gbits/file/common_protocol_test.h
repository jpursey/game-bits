#ifndef GBITS_FILE_COMMON_PROTOCOL_TEST_H_
#define GBITS_FILE_COMMON_PROTOCOL_TEST_H_

#include <memory>

#include "gbits/file/file_protocol.h"
#include "gtest/gtest.h"

namespace gb {

// Initialization structure passed to the CommonProtocolTestFactory function.
struct CommonProtocolTestInit {
  // Folders to create from root. These are ordered, such that parent folders
  // are always listed before child folders.
  std::vector<std::string> folders;

  // List of files to create, where the tuple is name to file contents. Files
  // may exist inside of the folders listed in this struct.
  std::vector<std::tuple<std::string, std::string>> files;

  // Initializes the protocol with this init structure using CreateFolder and
  // OpenFile. This allows the CommonProtocolTestFactory to be implemented like
  // this:
  //
  // std::unique_ptr<ExampleFileProtocol> ExampleFileProtocolFactory(
  //     const CommonProtocolTestInit& init) {
  //   return init.DefaultInit(std::make_unique<ExampleFileProtocol>());
  // }
  std::unique_ptr<FileProtocol> DefaultInit(
      std::unique_ptr<FileProtocol> protocol) const;
};

// Creates a file protocol that will be used with the CommonProtocolTest
// paramaterized test.
//
// Protocols initialized in this way are expected to be side-effect free after
// the returned protocol is destructed.
using CommonProtocolTestFactory =
    std::unique_ptr<FileProtocol> (*)(const CommonProtocolTestInit& init);

// Paramaterized test that does common testing for FileProtocol derived classes.
//
// The tests honor the FileProtocolFlags that are supported, only testing what
// is allowed. Protocols that don't support at least FileProtocolFlag::kInfo and
// FileProtocolFlag::kRead will be severely limited, as write and/or creation
// operations cannot be verified.
class CommonProtocolTest
    : public testing::TestWithParam<CommonProtocolTestFactory> {
 protected:
  CommonProtocolTest();

  std::unique_ptr<FileProtocol> NewProtocol(
      const CommonProtocolTestInit& init = {}) {
    return GetParam()(init);
  }
};

}  // namespace

#endif  // GBITS_FILE_COMMON_PROTOCOL_TEST_H_
