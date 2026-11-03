// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/volume_texture.h"

#include <algorithm>
#include <cstdint>

#include "absl/log/log.h"

namespace gb {

namespace {

// Clips the span of "size" texels at "pos" to [0, limit), returning how many
// texels were cut from its start, or -1 if nothing remains.
int ClipAxis(int* pos, int* size, int limit) {
  // The end is computed in 64 bits, as the span may be as large as an int
  // allows.
  const int64_t end = int64_t{*pos} + *size;
  if (*size <= 0 || *pos >= limit || end <= 0) {
    return -1;
  }
  int cut = 0;
  if (*pos < 0) {
    cut = -*pos;
    *pos = 0;
  }
  *size = static_cast<int>(std::min<int64_t>(end, limit)) - *pos;
  return cut;
}

}  // namespace

bool VolumeTexture::Clip(int* x, int* y, int* z, int* width, int* height,
                         int* depth, const void** texels) const {
  const int stride = *width;
  const int slice_height = *height;
  const int cut_x = ClipAxis(x, width, width_);
  const int cut_y = ClipAxis(y, height, height_);
  const int cut_z = ClipAxis(z, depth, depth_);
  if (cut_x < 0 || cut_y < 0 || cut_z < 0) {
    return true;
  }
  if (texels != nullptr) {
    const int64_t offset =
        ((int64_t{cut_z} * slice_height + cut_y) * stride + cut_x) *
        GetTexelSize(format_);
    *texels = static_cast<const uint8_t*>(*texels) + offset;
  }
  return false;
}

bool VolumeTexture::Clear(Pixel pixel) {
  return ClearRegion(0, 0, 0, width_, height_, depth_, pixel);
}

bool VolumeTexture::Set(const void* texels, int64_t size_in_bytes) {
  if (size_in_bytes < GetTextureSizeInBytes(format_, width_, height_, depth_)) {
    LOG(ERROR) << "Failed to set texels, as provided buffer is too small";
    return false;
  }
  return DoSet(0, 0, 0, width_, height_, depth_, texels, width_, height_);
}

bool VolumeTexture::Get(void* out_texels, int64_t size_in_bytes) const {
  if (volatility_ == DataVolatility::kStaticWrite) {
    LOG(ERROR)
        << "Volume texture cannot be read as its volatility is kStaticWrite";
    return false;
  }
  if (size_in_bytes < GetTextureSizeInBytes(format_, width_, height_, depth_)) {
    LOG(ERROR) << "Failed to get texels, as provided buffer is too small";
    return false;
  }
  return DoGet(out_texels);
}

bool VolumeTexture::ClearRegion(int x, int y, int z, int width, int height,
                                int depth, Pixel pixel) {
  if (Clip(&x, &y, &z, &width, &height, &depth)) {
    return true;
  }
  const uint32_t texel = PixelToTexel(format_, pixel);
  return DoClear(x, y, z, width, height, depth, &texel);
}

bool VolumeTexture::SetRegion(int x, int y, int z, int width, int height,
                              int depth, const void* texels,
                              int64_t size_in_bytes) {
  if (width <= 0 || height <= 0 || depth <= 0) {
    return true;
  }
  if (size_in_bytes < GetTextureSizeInBytes(format_, width, height, depth)) {
    LOG(ERROR) << "Failed to set texels, as provided buffer is too small";
    return false;
  }

  const int stride = width;
  const int slice_height = height;
  if (Clip(&x, &y, &z, &width, &height, &depth, &texels)) {
    return true;
  }
  return DoSet(x, y, z, width, height, depth, texels, stride, slice_height);
}

}  // namespace gb
