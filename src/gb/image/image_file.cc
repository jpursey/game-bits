// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/image/image_file.h"

#include "absl/log/log.h"
#include "gb/file/file.h"
#include "gb/file/file_system.h"
#include "stb_image.h"
#include "stb_image_write.h"

namespace gb {

std::unique_ptr<Image> LoadImage(File* file) {
  struct IoState {
    int64_t size;
    File* file;
  };
  static constexpr stbi_io_callbacks kIoCallbacks = {
      // Read callback.
      [](void* user, char* data, int size) -> int {
        auto* state = static_cast<IoState*>(user);
        return static_cast<int>(state->file->Read(data, size));
      },
      // Skip callback.
      [](void* user, int n) -> void {
        auto* state = static_cast<IoState*>(user);
        state->file->SeekBy(n);
      },
      // End-of-file callback.
      [](void* user) -> int {
        auto* state = static_cast<IoState*>(user);
        const int64_t position = state->file->GetPosition();
        return position < 0 || position == state->size;
      },
  };

  IoState state;
  file->SeekEnd();
  state.size = file->GetPosition();
  state.file = file;
  file->SeekBegin();

  int width;
  int height;
  int channels;
  void* pixels = stbi_load_from_callbacks(&kIoCallbacks, &state, &width,
                                          &height, &channels, STBI_rgb_alpha);

  // stb_image takes a failed read as the end of the file, so it may still
  // return an image, with what it couldn't read as zeros.
  if (!file->IsValid()) {
    stbi_image_free(pixels);
    LOG(ERROR) << "Failed to read image, as reading the file failed.";
    return nullptr;
  }
  if (pixels == nullptr) {
    LOG(ERROR) << "Failed to read image with error: " << stbi_failure_reason();
    return nullptr;
  }
  return std::make_unique<Image>(width, height, pixels, stbi_image_free);
}

std::unique_ptr<Image> LoadImage(gb::FileSystem* file_system,
                                 std::string_view filename) {
  auto file = file_system->OpenFile(filename, gb::kReadFileFlags);
  if (file == nullptr) {
    LOG(ERROR) << "Could not open image: " << filename;
    return nullptr;
  }
  return LoadImage(file.get());
}

bool SaveImage(File* file, const ImageView& image) {
  // A file opened without write access writes nothing and stays valid, so a
  // short write is what says writing failed.
  struct WriteState {
    File* file;
    bool failed;
  };
  static constexpr stbi_write_func* kWrite = [](void* context, void* data,
                                                int size) {
    auto* state = static_cast<WriteState*>(context);
    if (state->file->Write(data, size) != size) {
      state->failed = true;
    }
  };
  WriteState state = {file, false};
  const int result = stbi_write_png_to_func(
      kWrite, &state, image.GetWidth(), image.GetHeight(), 4,
      image.GetRawPixels(), image.GetWidth() * static_cast<int>(sizeof(Pixel)));
  if (result == 0) {
    LOG(ERROR) << "Failed to encode image as PNG.";
    return false;
  }
  if (state.failed) {
    LOG(ERROR) << "Failed to save image, as writing the file failed.";
    return false;
  }
  return true;
}

bool SaveImage(FileSystem* file_system, std::string_view filename,
               const ImageView& image) {
  auto file = file_system->OpenFile(filename, kNewFileFlags);
  if (file == nullptr) {
    LOG(ERROR) << "Could not create image: " << filename;
    return false;
  }
  if (!SaveImage(file.get(), image)) {
    return false;
  }
  if (!file->Close()) {
    LOG(ERROR) << "Failed to save image, as closing the file failed: "
               << filename;
    return false;
  }
  return true;
}

}  // namespace gb
