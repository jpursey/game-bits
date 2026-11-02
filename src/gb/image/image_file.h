// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMAGE_IMAGE_FILE_H_
#define GB_IMAGE_IMAGE_FILE_H_

#include <memory>
#include <string_view>

#include "gb/file/file_types.h"
#include "gb/image/image.h"
#include "gb/image/image_types.h"
#include "gb/image/image_view.h"

namespace gb {

// Loads an image from a file.
//
// This is implemented via the stb_image library, and so supports the image
// formats and limitations implied by it. See third_party/stb/stb_image.h for
// details.
//
// The image is read from the start of the file, wherever the file is
// positioned. If any error occurs while loading the file, this function will
// log an error and return null.
//
// This function is thread-compatible (relative to the file passed in).
std::unique_ptr<Image> LoadImage(File* file);
std::unique_ptr<Image> LoadImage(gb::FileSystem* file_system,
                                 std::string_view filename);

// Saves an image to a file as a PNG.
//
// The File version writes the PNG where the file is positioned, and leaves it
// open: the caller closes it, which can still fail. The FileSystem version
// creates the file (replacing any file already there), and closes it. If any
// error occurs while saving the image, these log an error and return false.
//
// This function is thread-compatible (relative to the file passed in).
bool SaveImage(File* file, const ImageView& image);
bool SaveImage(FileSystem* file_system, std::string_view filename,
               const ImageView& image);

}  // namespace gb

#endif  // GB_IMAGE_IMAGE_FILE_H_
