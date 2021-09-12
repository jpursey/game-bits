// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMAGE_IMAGE_FILE_H_
#define GB_IMAGE_IMAGE_FILE_H_

#include <string_view>

#include "gb/file/file_types.h"
#include "gb/image/image.h"

namespace gb {

// Loads an image from a file.
//
// This is implemented via the stb_image library, and so supports the image
// formats and limitations implied by it. See third_party/stb/stb_image.h for
// details.
//
// If any error occurs while loading the file, this function will log an error
// and return null.
//
// This function is thread-compatible (relative to the file passed in).
std::unique_ptr<Image> LoadImage(File* file);
std::unique_ptr<Image> LoadImage(gb::FileSystem* file_system,
                                 std::string_view filename);

}  // namespace gb

#endif  // GB_IMAGE_IMAGE_FILE_H_
