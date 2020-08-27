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

namespace gb {

namespace {

constexpr int kNeverUsedFrame = -1000;

struct DirtyRegion {
  DirtyRegion() = default;
  DirtyRegion(int x, int y, int width, int height)
      : x(x), y(y), width(width), height(height) {}

  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

inline void UnionDirtyRegion(DirtyRegion* region, int x, int y, int width,
                             int height) {
  const int x1 = std::min(x, region->x);
  const int y1 = std::min(y, region->y);
  const int x2 = std::max(x + width, region->x + region->width);
  const int y2 = std::max(y + height, region->y + region->height);
  *region = {x1, y1, x2 - x1, y2 - y1};
}

//==============================================================================
// VulkanStaticWriteTexture
//==============================================================================

class VulkanStaticWriteTexture final : public VulkanTexture {
 public:
  VulkanStaticWriteTexture(gb::ResourceEntry entry, VulkanBackend* backend,
                           vk::Sampler sampler, int width, int height)
      : VulkanTexture(std::move(entry), backend, sampler,
                      DataVolatility::kStaticWrite, width, height) {}

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

  for (const DirtyRegion& region : dirty_regions_) {
    state->image_updates.emplace_back(
        host_buffer_->Get(), image_->Get(), static_cast<uint32_t>(GetWidth()),
        static_cast<uint32_t>(GetHeight()), region.x, region.y,
        static_cast<uint32_t>(region.width),
        static_cast<uint32_t>(region.height));
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
                               vk::Sampler sampler, int width, int height)
      : VulkanTexture(std::move(entry), backend, sampler,
                      DataVolatility::kStaticReadWrite, width, height) {}

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

  state->image_updates.emplace_back(
      host_buffer_->Get(), image_->Get(), static_cast<uint32_t>(GetWidth()),
      static_cast<uint32_t>(GetHeight()), dirty_region_.x, dirty_region_.y,
      static_cast<uint32_t>(dirty_region_.width),
      static_cast<uint32_t>(dirty_region_.height));
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
                        vk::Sampler sampler, int width, int height)
      : VulkanTexture(std::move(entry), backend, sampler,
                      DataVolatility::kPerFrame, width, height) {}

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
  state->image_updates.emplace_back(
      frame_data.host->Get(), frame_data.image->Get(),
      static_cast<uint32_t>(GetWidth()), static_cast<uint32_t>(GetHeight()),
      region.x, region.y, static_cast<uint32_t>(region.width),
      static_cast<uint32_t>(region.height));
  frame_data.dirty_region = {};
  frame_data.dirty = false;
}

}  // namespace

VulkanTexture::~VulkanTexture() {}

VulkanTexture* VulkanTexture::Create(gb::ResourceEntry entry,
                                     VulkanBackend* backend,
                                     vk::Sampler sampler,
                                     DataVolatility volatility, int width,
                                     int height) {
  VulkanTexture* texture = nullptr;
  switch (volatility) {
    case DataVolatility::kStaticWrite:
      texture = new VulkanStaticWriteTexture(std::move(entry), backend, sampler,
                                             width, height);
      break;
    case DataVolatility::kStaticReadWrite:
      texture = new VulkanStaticReadWriteTexture(std::move(entry), backend,
                                                 sampler, width, height);
      break;
    case DataVolatility::kPerFrame:
      texture = new VulkanPerFrameTexture(std::move(entry), backend, sampler,
                                          width, height);
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
      backend_, GetWidth(), GetHeight(), vk::Format::eR8G8B8A8Srgb,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
}

std::unique_ptr<VulkanBuffer> VulkanTexture::CreateHostBuffer() {
  return VulkanBuffer::Create(
      backend_,
      vk::BufferCreateInfo()
          .setSize(GetWidth() * GetHeight() * sizeof(Pixel))
          .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
          .setSharingMode(vk::SharingMode::eExclusive),
      VMA_MEMORY_USAGE_CPU_ONLY);
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
