// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_texels.h"

#include <algorithm>
#include <cstring>

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "gb/render/webgpu/webgpu_format.h"

namespace gb {

std::unique_ptr<WebGpuTexels> WebGpuTexels::Create(
    WebGpuBackend* backend, DataVolatility volatility,
    const Properties& properties) {
  if (properties.format == TextureFormat::kRgba4 ||
      properties.format == TextureFormat::kDepth) {
    LOG(ERROR) << "The WebGPU backend doesn't support textures of format "
               << static_cast<int>(properties.format) << " yet";
    return nullptr;
  }

  wgpu::TextureDescriptor descriptor;
  descriptor.usage =
      wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  descriptor.dimension = properties.dimension;
  descriptor.size = {static_cast<uint32_t>(properties.width),
                     static_cast<uint32_t>(properties.height),
                     static_cast<uint32_t>(properties.depth)};
  descriptor.format = GetWebGpuFormat(properties.format);
  wgpu::Texture texture = backend->GetDevice().CreateTexture(&descriptor);
  if (texture == nullptr) {
    LOG(ERROR) << "Failed to create a WebGPU texture";
    return nullptr;
  }
  return absl::WrapUnique(
      new WebGpuTexels(backend, volatility, properties, std::move(texture)));
}

WebGpuTexels::WebGpuTexels(WebGpuBackend* backend, DataVolatility volatility,
                           const Properties& properties, wgpu::Texture texture)
    : backend_(backend),
      format_(properties.format),
      width_(properties.width),
      height_(properties.height),
      depth_(properties.depth),
      texel_size_(GetTexelSize(properties.format)),
      gpu_bytes_(int64_t{properties.width} * properties.height *
                 properties.depth * GetWebGpuTexelSize(properties.format)),
      texture_(std::move(texture)) {
  if (volatility != DataVolatility::kStaticWrite) {
    texels_.resize(GetLayerSize() * depth_);
  }
  backend_->AddAllocatedBytes(gpu_bytes_);
}

WebGpuTexels::~WebGpuTexels() {
  backend_->RemovePendingWrite(this);
  texture_.Destroy();
  backend_->AddAllocatedBytes(-gpu_bytes_);
}

bool WebGpuTexels::Clear(const Region& region, const void* texel) {
  if (texels_.empty()) {
    std::vector<uint8_t> texels(GetTextureSizeInBytes(
        format_, region.width, region.height, region.depth));
    const int64_t size = static_cast<int64_t>(texels.size());
    for (int64_t i = 0; i < size; i += texel_size_) {
      std::memcpy(texels.data() + i, texel, texel_size_);
    }
    WriteRegion(backend_->GetQueue(), region, texels.data(), region.width,
                region.height);
    return true;
  }
  for (int z = region.z; z < region.z + region.depth; ++z) {
    for (int y = region.y; y < region.y + region.height; ++y) {
      uint8_t* row = texels_.data() + z * GetLayerSize() +
                     (int64_t{y} * width_ + region.x) * texel_size_;
      for (int x = 0; x < region.width; ++x) {
        std::memcpy(row + x * texel_size_, texel, texel_size_);
      }
    }
  }
  MarkPending(region);
  return true;
}

bool WebGpuTexels::Set(const Region& region, const void* texels, int stride,
                       int slice_height) {
  if (texels_.empty()) {
    WriteRegion(backend_->GetQueue(), region, texels, stride, slice_height);
    return true;
  }
  const uint8_t* src = static_cast<const uint8_t*>(texels);
  const int64_t row_bytes = int64_t{region.width} * texel_size_;
  for (int z = 0; z < region.depth; ++z) {
    for (int y = 0; y < region.height; ++y) {
      const uint8_t* src_row =
          src + (int64_t{z} * slice_height + y) * stride * texel_size_;
      uint8_t* dst_row =
          texels_.data() + (region.z + z) * GetLayerSize() +
          (int64_t{region.y + y} * width_ + region.x) * texel_size_;
      std::memcpy(dst_row, src_row, row_bytes);
    }
  }
  MarkPending(region);
  return true;
}

bool WebGpuTexels::Get(void* out_texels) const {
  if (texels_.empty()) {
    return false;
  }
  std::memcpy(out_texels, texels_.data(), texels_.size());
  return true;
}

bool WebGpuTexels::GetLayer(int z, void* out_texels) const {
  if (texels_.empty()) {
    return false;
  }
  std::memcpy(out_texels, texels_.data() + z * GetLayerSize(), GetLayerSize());
  return true;
}

void* WebGpuTexels::EditBegin() {
  if (texels_.empty()) {
    return nullptr;
  }
  return texels_.data();
}

void WebGpuTexels::EditEnd(bool modified) {
  if (modified) {
    MarkPending(GetWholeRegion());
  }
}

void WebGpuTexels::MarkPending(const Region& region) {
  if (!pending_.has_value()) {
    pending_ = region;
    backend_->AddPendingWrite(this);
    return;
  }
  Region& pending = *pending_;
  const int x1 = std::max(pending.x + pending.width, region.x + region.width);
  const int y1 = std::max(pending.y + pending.height, region.y + region.height);
  const int z1 = std::max(pending.z + pending.depth, region.z + region.depth);
  pending.x = std::min(pending.x, region.x);
  pending.y = std::min(pending.y, region.y);
  pending.z = std::min(pending.z, region.z);
  pending.width = x1 - pending.x;
  pending.height = y1 - pending.y;
  pending.depth = z1 - pending.z;
}

void WebGpuTexels::WritePending(const wgpu::Queue& queue) {
  if (!pending_.has_value()) {
    return;
  }
  const Region& region = *pending_;
  const uint8_t* first = texels_.data() + region.z * GetLayerSize() +
                         (int64_t{region.y} * width_ + region.x) * texel_size_;
  WriteRegion(queue, region, first, width_, height_);
  pending_.reset();
}

void WebGpuTexels::WriteRegion(const wgpu::Queue& queue, const Region& region,
                               const void* texels, int stride,
                               int slice_height) {
  wgpu::TexelCopyTextureInfo destination;
  destination.texture = texture_;
  destination.origin = {static_cast<uint32_t>(region.x),
                        static_cast<uint32_t>(region.y),
                        static_cast<uint32_t>(region.z)};
  wgpu::TexelCopyBufferLayout layout;
  layout.bytesPerRow = static_cast<uint32_t>(stride * texel_size_);
  layout.rowsPerImage = static_cast<uint32_t>(slice_height);
  const wgpu::Extent3D size = {static_cast<uint32_t>(region.width),
                               static_cast<uint32_t>(region.height),
                               static_cast<uint32_t>(region.depth)};

  // The data ends with the region's last texel.
  const int64_t data_size =
      ((int64_t{region.depth - 1} * slice_height + region.height - 1) * stride +
       region.width) *
      texel_size_;
  queue.WriteTexture(&destination, texels, static_cast<size_t>(data_size),
                     &layout, &size);
}

}  // namespace gb
