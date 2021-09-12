// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/image/image_file.h"

#include "gb/file/file.h"
#include "gb/file/file_system.h"
#include "glog/logging.h"
#include "stb_image.h"

namespace gb {

std::unique_ptr<Image> LoadImage(File* file) {
  struct IoState {
    int64_t size;
    File* file;
  };
  static stbi_io_callbacks io_callbacks = {
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
  void* pixels = stbi_load_from_callbacks(&io_callbacks, &state, &width,
                                          &height, &channels, STBI_rgb_alpha);
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

}  // namespace gb
