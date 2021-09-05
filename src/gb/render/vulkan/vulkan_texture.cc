// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_texture.h"

#include <algorithm>

#include "absl/container/inlined_vector.h"
#include "absl/memory/memory.h"
#include "gb/base/scoped_call.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_buffer.h"
#include "glog/logging.h"
#include "stb_image_resize.h"

namespace gb {

namespace {

constexpr int kNeverUsedFrame = -1000;

//==============================================================================
// VulkanStaticWriteTexture
//==============================================================================

class VulkanStaticWriteTexture final : public VulkanTexture {
 public:
  VulkanStaticWriteTexture(gb::ResourceEntry entry, VulkanBackend* backend,
                           vk::Sampler sampler, int width, int height,
                           const SamplerOptions& options)
      : VulkanTexture(std::move(entry), backend, sampler,
                      DataVolatility::kStaticWrite, width, height, options) {}

 protected:
  bool Init() override;
  bool DoClear(int x, int y, int width, int height, Pixel pixel) override;
  bool DoSet(int x, int y, int width, int height, const void* pixels,
             int stride) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;

 private:
  ~VulkanStaticWriteTexture() override;

  void AddDirtyRegion(int x, int y, int width, int height);

  bool dirty_ = false;
  absl::InlinedVector<DirtyRegion, 1> dirty_regions_;
  int render_frame_ = kNeverUsedFrame;
  std::unique_ptr<VulkanImage> image_;
  std::unique_ptr<VulkanBuffer> host_buffer_;
};

VulkanStaticWriteTexture::~VulkanStaticWriteTexture() {}

bool VulkanStaticWriteTexture::Init() {
  image_ = CreateImage();
  if (image_ == nullptr) {
    return false;
  }
  SetAllImageHandles(image_.get());
  return true;
}

bool VulkanStaticWriteTexture::DoClear(int x, int y, int width, int height,
                                       Pixel pixel) {
  if (dirty_) {
    ClearRegion(host_buffer_->GetData(), x, y, width, height, pixel);
    AddDirtyRegion(x, y, width, height);
    return true;
  }

  host_buffer_ = CreateHostBuffer();
  if (host_buffer_ == nullptr) {
    return false;
  }

  if (FrameInUse(render_frame_)) {
    if (x != 0 || y != 0 || width != GetWidth() || height != GetHeight()) {
      host_buffer_.reset();
      return false;
    }

    auto image = CreateImage();
    if (image == nullptr) {
      host_buffer_.reset();
      return false;
    }
    image_ = std::move(image);
    SetAllImageHandles(image_.get());
    render_frame_ = kNeverUsedFrame;
  }

  ClearRegion(host_buffer_->GetData(), x, y, width, height, pixel);
  AddDirtyRegion(x, y, width, height);
  dirty_ = true;
  return true;
}

bool VulkanStaticWriteTexture::DoSet(int x, int y, int width, int height,
                                     const void* pixels, int stride) {
  if (dirty_) {
    CopyRegion(host_buffer_->GetData(), x, y, width, height, pixels, stride);
    AddDirtyRegion(x, y, width, height);
    return true;
  }

  host_buffer_ = CreateHostBuffer();
  if (host_buffer_ == nullptr) {
    return false;
  }

  if (FrameInUse(render_frame_)) {
    if (x != 0 || y != 0 || width != GetWidth() || height != GetHeight()) {
      host_buffer_.reset();
      return false;
    }

    auto image = CreateImage();
    if (image == nullptr) {
      host_buffer_.reset();
      return false;
    }
    image_ = std::move(image);
    SetAllImageHandles(image_.get());
    render_frame_ = kNeverUsedFrame;
  }

  CopyRegion(host_buffer_->GetData(), x, y, width, height, pixels, stride);
  AddDirtyRegion(x, y, width, height);
  dirty_ = true;
  return true;
}

void* VulkanStaticWriteTexture::DoEditBegin() { return nullptr; }
void VulkanStaticWriteTexture::OnEditEnd(bool modified) {}

void VulkanStaticWriteTexture::DoRender(VulkanRenderState* state) {
  render_frame_ = state->frame;

  if (!dirty_) {
    return;
  }

  int num_regions = static_cast<int>(dirty_regions_.size());
  for (int i = 0; i < num_regions; ++i) {
    UpdateImage(state, host_buffer_.get(), image_.get(), dirty_regions_[i],
                i == num_regions - 1);
  }
  host_buffer_.reset();
  dirty_regions_.clear();
  dirty_ = false;
}

void VulkanStaticWriteTexture::AddDirtyRegion(int x, int y, int width,
                                              int height) {
  if (dirty_regions_.empty()) {
    dirty_regions_.emplace_back(x, y, width, height);
    return;
  }

  // Skip this region if it is fully contained in an existing region.
  for (const DirtyRegion& region : dirty_regions_) {
    if (x >= region.x && y >= region.y &&
        x + width <= region.x + region.width &&
        y + height <= region.y + region.height) {
      return;
    }
  }

  // Remove any regions fully contained in this region.
  auto end = std::remove_if(dirty_regions_.begin(), dirty_regions_.end(),
                            [x, y, width, height](const DirtyRegion& region) {
                              return region.x >= x && region.y <= y &&
                                     region.x + region.width <= x + width &&
                                     region.y + region.height <= y + height;
                            });
  if (end != dirty_regions_.end()) {
    dirty_regions_.resize(end - dirty_regions_.begin());
  }

  dirty_regions_.emplace_back(x, y, width, height);
}

//==============================================================================
// VulkanStaticReadWriteTexture
//==============================================================================

class VulkanStaticReadWriteTexture final : public VulkanTexture {
 public:
  VulkanStaticReadWriteTexture(gb::ResourceEntry entry, VulkanBackend* backend,
                               vk::Sampler sampler, int width, int height,
                               const SamplerOptions& options)
      : VulkanTexture(std::move(entry), backend, sampler,
                      DataVolatility::kStaticReadWrite, width, height,
                      options) {}

 protected:
  bool Init() override;
  bool DoClear(int x, int y, int width, int height, Pixel pixel) override;
  bool DoSet(int x, int y, int width, int height, const void* pixels,
             int stride) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;

 private:
  ~VulkanStaticReadWriteTexture() override;

  bool EnsureHostBufferIsWritable(bool copy_contents);
  bool EnsureImageIsWritable();

  bool was_dirty_ = false;
  bool dirty_ = false;
  DirtyRegion dirty_region_;
  int render_frame_ = kNeverUsedFrame;
  int transfer_frame_ = kNeverUsedFrame;
  std::unique_ptr<VulkanImage> image_;
  std::unique_ptr<VulkanBuffer> host_buffer_;
};

VulkanStaticReadWriteTexture::~VulkanStaticReadWriteTexture() {}

bool VulkanStaticReadWriteTexture::Init() {
  image_ = CreateImage();
  host_buffer_ = CreateHostBuffer();
  if (image_ == nullptr || host_buffer_ == nullptr) {
    return false;
  }
  SetAllImageHandles(image_.get());
  return true;
}

bool VulkanStaticReadWriteTexture::DoClear(int x, int y, int width, int height,
                                           Pixel pixel) {
  if (const bool copy_contents =
          x != 0 || y != 0 || width != GetWidth() || height != GetHeight();
      !EnsureHostBufferIsWritable(copy_contents)) {
    return false;
  }
  ClearRegion(host_buffer_->GetData(), x, y, width, height, pixel);

  if (dirty_) {
    // TODO: If performance warrants it, we could maintain a list of
    //       non-intersecting regions to update instead of just unioning all
    //       regions together.
    UnionDirtyRegion(&dirty_region_, x, y, width, height);
    return true;
  }

  if (!EnsureImageIsWritable()) {
    return false;
  }
  dirty_ = true;
  dirty_region_ = {x, y, width, height};
  return true;
}

bool VulkanStaticReadWriteTexture::DoSet(int x, int y, int width, int height,
                                         const void* pixels, int stride) {
  if (const bool copy_contents =
          x != 0 || y != 0 || width != GetWidth() || height != GetHeight();
      !EnsureHostBufferIsWritable(copy_contents)) {
    return false;
  }
  CopyRegion(host_buffer_->GetData(), x, y, width, height, pixels, stride);

  if (dirty_) {
    // TODO: If performance warrants it, we could maintain a list of
    //       non-intersecting regions to update instead of just unioning all
    //       regions together.
    UnionDirtyRegion(&dirty_region_, x, y, width, height);
    return true;
  }

  if (!EnsureImageIsWritable()) {
    return false;
  }
  dirty_ = true;
  dirty_region_ = {x, y, width, height};
  return true;
}

void* VulkanStaticReadWriteTexture::DoEditBegin() {
  if (!EnsureHostBufferIsWritable(true)) {
    return nullptr;
  }

  // Explicitly clear the dirty flag, as we do not want to initiate a transfer
  // until OnEditEnd is called.
  was_dirty_ = dirty_;
  dirty_ = false;
  return host_buffer_->GetData();
}

void VulkanStaticReadWriteTexture::OnEditEnd(bool modified) {
  if (!modified) {
    dirty_ = was_dirty_;
    return;
  }
  if (!EnsureImageIsWritable()) {
    return;
  }
  dirty_ = true;
  dirty_region_ = {0, 0, GetWidth(), GetHeight()};
}

void VulkanStaticReadWriteTexture::DoRender(VulkanRenderState* state) {
  render_frame_ = state->frame;

  if (!dirty_) {
    return;
  }

  UpdateImage(state, host_buffer_.get(), image_.get(), dirty_region_);
  transfer_frame_ = render_frame_;
  dirty_region_ = {};
  dirty_ = false;
}

bool VulkanStaticReadWriteTexture::EnsureHostBufferIsWritable(
    bool copy_contents) {
  if (!FrameInUse(transfer_frame_)) {
    return true;
  }

  auto host_buffer = CreateHostBuffer();
  if (host_buffer == nullptr) {
    return false;
  }
  if (copy_contents) {
    std::memcpy(host_buffer->GetData(), host_buffer_->GetData(),
                host_buffer_->GetSize());
  }
  host_buffer_ = std::move(host_buffer);

  // This host buffer has never been transferred.
  transfer_frame_ = kNeverUsedFrame;
  return true;
}

bool VulkanStaticReadWriteTexture::EnsureImageIsWritable() {
  if (!FrameInUse(render_frame_)) {
    return true;
  }

  auto image = CreateImage();
  if (image == nullptr) {
    return false;
  }
  image_ = std::move(image);
  SetAllImageHandles(image_.get());

  // This image has never been rendererd.
  render_frame_ = kNeverUsedFrame;
  return true;
}

//==============================================================================
// VulkanPerFrameTexture
//==============================================================================

class VulkanPerFrameTexture final : public VulkanTexture {
 public:
  VulkanPerFrameTexture(gb::ResourceEntry entry, VulkanBackend* backend,
                        vk::Sampler sampler, int width, int height,
                        const SamplerOptions& options)
      : VulkanTexture(std::move(entry), backend, sampler,
                      DataVolatility::kPerFrame, width, height, options) {}

 protected:
  bool Init() override;
  bool DoClear(int x, int y, int width, int height, Pixel pixel) override;
  bool DoSet(int x, int y, int width, int height, const void* pixels,
             int stride) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;

 private:
  struct FrameData {
    FrameData() = default;
    bool dirty = false;
    DirtyRegion dirty_region = {};
    std::unique_ptr<VulkanBuffer> host;
    std::unique_ptr<VulkanImage> image;
  };

  ~VulkanPerFrameTexture() override;

  void* local_buffer_ = nullptr;
  FrameData frame_data_[kMaxFramesInFlight];
};

VulkanPerFrameTexture::~VulkanPerFrameTexture() { std::free(local_buffer_); }

bool VulkanPerFrameTexture::Init() {
  for (auto& frame_data : frame_data_) {
    frame_data.host = CreateHostBuffer();
    frame_data.image = CreateImage();
    if (frame_data.host == nullptr || !frame_data.image) {
      return false;
    }
  }
  local_buffer_ = std::calloc(GetWidth() * GetHeight() * sizeof(Pixel), 1);
  if (local_buffer_ == nullptr) {
    return false;
  }
  for (int i = 0; i < kMaxFramesInFlight; ++i) {
    SetImageHandle(i, frame_data_[i].image.get());
  }
  return true;
}

bool VulkanPerFrameTexture::DoClear(int x, int y, int width, int height,
                                    Pixel pixel) {
  ClearRegion(local_buffer_, x, y, width, height, pixel);
  for (auto& frame_data : frame_data_) {
    if (frame_data.dirty) {
      UnionDirtyRegion(&frame_data.dirty_region, x, y, width, height);
    } else {
      frame_data.dirty_region = {x, y, width, height};
      frame_data.dirty = true;
    }
  }
  return true;
}

bool VulkanPerFrameTexture::DoSet(int x, int y, int width, int height,
                                  const void* pixels, int stride) {
  CopyRegion(local_buffer_, x, y, width, height, pixels, stride);
  for (auto& frame_data : frame_data_) {
    if (frame_data.dirty) {
      UnionDirtyRegion(&frame_data.dirty_region, x, y, width, height);
    } else {
      frame_data.dirty_region = {x, y, width, height};
      frame_data.dirty = true;
    }
  }
  return true;
}

void* VulkanPerFrameTexture::DoEditBegin() { return local_buffer_; }

void VulkanPerFrameTexture::OnEditEnd(bool modified) {
  if (modified) {
    for (auto& frame_data : frame_data_) {
      frame_data.dirty_region = {0, 0, GetWidth(), GetHeight()};
      frame_data.dirty = true;
    }
  }
}

void VulkanPerFrameTexture::DoRender(VulkanRenderState* state) {
  auto& frame_data = frame_data_[state->frame % kMaxFramesInFlight];
  if (!frame_data.dirty) {
    return;
  }

  const auto& region = frame_data.dirty_region;
  CopyRegion(frame_data.host->GetData(), region.x, region.y, region.width,
             region.height,
             static_cast<const Pixel*>(local_buffer_) +
                 (region.y * GetWidth() + region.x),
             GetWidth());
  UpdateImage(state, frame_data.host.get(), frame_data.image.get(), region);
  frame_data.dirty_region = {};
  frame_data.dirty = false;
}

}  // namespace

VulkanTexture::VulkanTexture(gb::ResourceEntry entry, VulkanBackend* backend,
                             vk::Sampler sampler, DataVolatility volatility,
                             int width, int height,
                             const SamplerOptions& options)
    : Texture(std::move(entry), volatility, width, height, options),
      backend_(backend),
      sampler_(sampler) {
  host_size_ = width * height * sizeof(Pixel);
  if (options.mipmap) {
    int size = options.tile_size;
    if (size == 0) {
      size = std::min(GetWidth(), GetHeight());
    }
    size >>= 1;
    while (size != 0) {
      host_size_ +=
          (width >> mip_levels_) * (height >> mip_levels_) * sizeof(Pixel);
      size >>= 1;
      ++mip_levels_;
    }
  }
}

VulkanTexture::~VulkanTexture() {}

VulkanTexture* VulkanTexture::Create(gb::ResourceEntry entry,
                                     VulkanBackend* backend,
                                     vk::Sampler sampler,
                                     DataVolatility volatility, int width,
                                     int height,
                                     const SamplerOptions& options) {
  VulkanTexture* texture = nullptr;
  switch (volatility) {
    case DataVolatility::kStaticWrite:
      texture = new VulkanStaticWriteTexture(std::move(entry), backend, sampler,
                                             width, height, options);
      break;
    case DataVolatility::kStaticReadWrite:
      texture = new VulkanStaticReadWriteTexture(
          std::move(entry), backend, sampler, width, height, options);
      break;
    case DataVolatility::kPerFrame:
      texture = new VulkanPerFrameTexture(std::move(entry), backend, sampler,
                                          width, height, options);
      break;
    default:
      LOG(ERROR) << "Unhandled data volatility for texture";
      return nullptr;
  }
  if (!texture->Init()) {
    texture->Delete();
    return nullptr;
  }
  return texture;
}

std::unique_ptr<VulkanImage> VulkanTexture::CreateImage() {
  return VulkanImage::Create(
      backend_, GetWidth(), GetHeight(), 1, vk::Format::eR8G8B8A8Srgb,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      VulkanImage::Options().SetMipLevels(mip_levels_));
}

std::unique_ptr<VulkanBuffer> VulkanTexture::CreateHostBuffer() {
  return VulkanBuffer::Create(
      backend_,
      vk::BufferCreateInfo()
          .setSize(host_size_)
          .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
          .setSharingMode(vk::SharingMode::eExclusive),
      VMA_MEMORY_USAGE_CPU_ONLY);
}

void VulkanTexture::UpdateImage(VulkanRenderState* state,
                                VulkanBuffer* host_buffer, VulkanImage* image,
                                const DirtyRegion& region, bool update_mips) {
  state->image_updates.emplace_back(
      host_buffer->Get(), /*src_offset=*/0, image->Get(), /*mip_level=*/0,
      GetWidth(), GetHeight(),
      /*image_layer=*/0, region.x, region.y, region.width, region.height);
  if (update_mips) {
    state->image_barriers.emplace_back(image->Get(), mip_levels_);
  }
  if (!update_mips || mip_levels_ == 1) {
    return;
  }

  int src_tile_size = GetSamplerOptions().tile_size;
  int src_width = GetWidth();
  int src_height = GetHeight();
  uint32_t offset = src_width * src_height * sizeof(Pixel);
  unsigned char* src = static_cast<unsigned char*>(host_buffer->GetData());
  unsigned char* dst = src + offset;
  for (int mip = 1; mip < mip_levels_; ++mip) {
    int dst_width = src_width >> 1;
    int dst_height = src_height >> 1;
    uint32_t mip_byte_size = dst_width * dst_height * sizeof(Pixel);

    if (src_tile_size == 0) {
      stbir_resize_uint8_srgb(src, src_width, src_height, 0, dst, dst_width,
                              dst_height, 0, 4, 3, 0);
    } else {
      int dst_tile_size = src_tile_size >> 1;
      int src_tile_stride = src_width * sizeof(Pixel);
      int dst_tile_stride = dst_width * sizeof(Pixel);
      for (int y = 0; y < src_height; y += src_tile_size) {
        unsigned char* tile_src = src + (src_tile_stride * y);
        unsigned char* tile_dst = dst + (dst_tile_stride * y / 2);
        for (int x = 0; x < src_width; x += src_tile_size) {
          stbir_resize_uint8_srgb(tile_src, src_tile_size, src_tile_size,
                                  src_tile_stride, tile_dst, dst_tile_size,
                                  dst_tile_size, dst_tile_stride, 4, 3, 0);
          tile_src += src_tile_size * sizeof(Pixel);
          tile_dst += dst_tile_size * sizeof(Pixel);
        }
      }
      src_tile_size = dst_tile_size;
    }

    state->image_updates.emplace_back(host_buffer->Get(), offset, image->Get(),
                                      mip, dst_width, dst_height);

    src = dst;
    src_width = dst_width;
    src_height = dst_height;

    offset += mip_byte_size;
    dst += mip_byte_size;
  }
}

void VulkanTexture::ClearRegion(void* buffer, int x, int y, int width,
                                int height, Pixel pixel) {
  Pixel* dst = static_cast<Pixel*>(buffer) + (y * GetWidth() + x);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      dst[x] = pixel;
    }
    dst += GetWidth();
  }
}

void VulkanTexture::CopyRegion(void* buffer, int x, int y, int width,
                               int height, const void* pixels, int stride) {
  Pixel* dst = static_cast<Pixel*>(buffer) + (y * GetWidth() + x);
  const Pixel* src = static_cast<const Pixel*>(pixels);
  if (stride == width) {
    std::memcpy(dst, src, width * height * sizeof(Pixel));
    return;
  }
  for (int row = 0; row < height; ++row) {
    std::memcpy(dst, src, width * sizeof(Pixel));
    dst += GetWidth();
    src += stride;
  }
}

}  // namespace gb
