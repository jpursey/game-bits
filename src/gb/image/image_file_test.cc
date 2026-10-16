// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/image/image_file.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>

#include "gb/file/file.h"
#include "gb/file/file_system.h"
#include "gb/file/memory_file_protocol.h"
#include "gb/file/test_protocol.h"
#include "gb/test/log_error_guard.h"
#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::HasSubstr;

// The ID field is longer than stb_image's read buffer, so loading the image
// skips past what it has read.
constexpr int kTgaIdSize = 200;

// The header of a 2x2 32-bit TGA file: the size of its ID, no color map,
// uncompressed true color, an empty color map specification, the origin, the
// width and height, 32 bits per pixel, and rows from the top with 8 bits of
// alpha.
constexpr uint8_t kTgaHeader[] = {kTgaIdSize, 0, 2, 0, 0, 0, 0, 0,  0,
                                  0,          0, 0, 2, 0, 2, 0, 32, 0x28};

// The pixels of the TGA file, in order from the top left.
constexpr Pixel kTgaPixels[4] = {
    {1, 2, 3, 4},
    {5, 6, 7, 8},
    {9, 10, 11, 12},
    {13, 14, 15, 16},
};

// Returns a TGA file of kTgaPixels.
std::string MakeTgaFile() {
  std::string file(std::begin(kTgaHeader), std::end(kTgaHeader));
  file.append(kTgaIdSize, 'x');
  for (const Pixel& pixel : kTgaPixels) {
    file.push_back(static_cast<char>(pixel.b));
    file.push_back(static_cast<char>(pixel.g));
    file.push_back(static_cast<char>(pixel.r));
    file.push_back(static_cast<char>(pixel.a));
  }
  return file;
}

class ImageFileTest : public ::testing::Test {
 protected:
  ImageFileTest() {
    file_system_.Register(std::make_unique<MemoryFileProtocol>());
    file_system_.Register(std::make_unique<TestProtocol>(&test_state_), "test");
  }

  LogErrorGuard log_error_guard_;
  TestProtocol::State test_state_;
  FileSystem file_system_;
};

TEST_F(ImageFileTest, LoadImageFromFileSystem) {
  ASSERT_TRUE(file_system_.WriteFile("mem:/image.tga", MakeTgaFile()));
  auto image = LoadImage(&file_system_, "mem:/image.tga");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->GetWidth(), 2);
  EXPECT_EQ(image->GetHeight(), 2);
  EXPECT_THAT(image->GetPixels(), ElementsAreArray(kTgaPixels));
}

TEST_F(ImageFileTest, LoadImageFromFile) {
  ASSERT_TRUE(file_system_.WriteFile("mem:/image.tga", MakeTgaFile()));
  auto file = file_system_.OpenFile("mem:/image.tga", kReadFileFlags);
  ASSERT_NE(file, nullptr);

  // The image is read from the start of the file, wherever it is positioned.
  file->SeekTo(10);
  auto image = LoadImage(file.get());
  ASSERT_NE(image, nullptr);
  EXPECT_THAT(image->GetPixels(), ElementsAreArray(kTgaPixels));
}

TEST_F(ImageFileTest, LoadMissingImage) {
  EXPECT_EQ(LoadImage(&file_system_, "mem:/missing.tga"), nullptr);
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              Contains(Field(&LogRecorder::Message::text,
                             HasSubstr("Could not open image"))));
}

TEST_F(ImageFileTest, LoadInvalidImage) {
  ASSERT_TRUE(file_system_.WriteFile("mem:/image.tga", "Not an image"));
  EXPECT_EQ(LoadImage(&file_system_, "mem:/image.tga"), nullptr);
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text,
                                HasSubstr("Failed to read image"))));
}

TEST_F(ImageFileTest, LoadEmptyImage) {
  ASSERT_TRUE(file_system_.WriteFile("mem:/image.tga", ""));
  EXPECT_EQ(LoadImage(&file_system_, "mem:/image.tga"), nullptr);
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text,
                                HasSubstr("Failed to read image"))));
}

TEST_F(ImageFileTest, LoadImageWithFailedRead) {
  test_state_.paths["/image.tga"] =
      TestProtocol::PathState::NewFile(MakeTgaFile());

  // stb_image reads 128 bytes at a time, which is the header and most of the
  // ID, then skips the rest of the ID. The read of the pixels fails.
  TestProtocol::FileState* file_state =
      test_state_.paths["/image.tga"].GetFile();
  file_state->fail_read_after = 128;
  EXPECT_EQ(LoadImage(&file_system_, "test:/image.tga"), nullptr);
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text,
                                HasSubstr("reading the file failed"))));

  // The read failed part way through the image.
  EXPECT_GT(file_state->bytes_read, static_cast<int64_t>(sizeof(kTgaHeader)));
}

TEST_F(ImageFileTest, LoadImageWithFailedFirstRead) {
  test_state_.paths["/image.tga"] =
      TestProtocol::PathState::NewFile(MakeTgaFile());

  // The error is the failed read, rather than the unknown format stb_image
  // finds in what it read.
  test_state_.paths["/image.tga"].GetFile()->fail_read_after = 0;
  EXPECT_EQ(LoadImage(&file_system_, "test:/image.tga"), nullptr);
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text,
                                HasSubstr("reading the file failed"))));
}

}  // namespace
}  // namespace gb
