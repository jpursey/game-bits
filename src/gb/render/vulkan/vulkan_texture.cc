// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_texture.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "absl/container/inlined_vector.h"
#include "absl/log/log.h"
#include "gb/render/mipmaps.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_buffer.h"
#include "gb/render/vulkan/vulkan_format.h"

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
                           const SamplerOptions& options, TextureFormat format)
      : VulkanTexture(std::move(entry), backend, sampler,
                      DataVolatility::kStaticWrite, width, height, options,
                      format) {}

 protected:
  bool Init() override;
  bool DoClear(int x, int y, int width, int height, const void* texel) override;
  bool DoSet(int x, int y, int width, int height, const void* texels,
             int stride) override;
  bool DoGet(void* out_texels) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;

 private:
  ~VulkanStaticWriteTexture() override;

  bool PrepForWrite(int x, int y, int width, int height);
  void AddDirtyRegion(int x, int y, int width, int height);

  absl::InlinedVector<DirtyRegion, 1> dirty_regions_;
  int render_frame_ = kNeverUsedFrame;
  std::unique_ptr<VulkanImage> image_;

  // Holds the pixels written since the last upload, and is null if there are
  // none.
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

bool VulkanStaticWriteTexture::PrepForWrite(int x, int y, int width,
                                            int height) {
  if (host_buffer_ != nullptr) {
    return true;
  }

  // The texture's pixels can't be read back, so a partial update can't be
  // applied to a new image (which replaces one in use), or used to generate
  // mipmaps from.
  const bool in_use = FrameInUse(render_frame_);
  const bool partial =
      x != 0 || y != 0 || width != GetWidth() || height != GetHeight();
  if (partial && (in_use || GetMipLevels() > 1)) {
    return false;
  }

  host_buffer_ = CreateHostBuffer();
  if (host_buffer_ == nullptr) {
    return false;
  }

  if (in_use) {
    auto image = CreateImage();
    if (image == nullptr) {
      host_buffer_.reset();
      return false;
    }
    image_ = std::move(image);
    SetAllImageHandles(image_.get());
    render_frame_ = kNeverUsedFrame;
  }
  return true;
}

bool VulkanStaticWriteTexture::DoClear(int x, int y, int width, int height,
                                       const void* texel) {
  if (!PrepForWrite(x, y, width, height)) {
    return false;
  }
  ClearRegion(host_buffer_->GetData(), x, y, width, height, texel);
  AddDirtyRegion(x, y, width, height);
  return true;
}

bool VulkanStaticWriteTexture::DoSet(int x, int y, int width, int height,
                                     const void* texels, int stride) {
  if (!PrepForWrite(x, y, width, height)) {
    return false;
  }
  CopyRegion(host_buffer_->GetData(), x, y, width, height, texels, stride);
  AddDirtyRegion(x, y, width, height);
  return true;
}

bool VulkanStaticWriteTexture::DoGet(void* out_texels) { return false; }
void* VulkanStaticWriteTexture::DoEditBegin() { return nullptr; }
void VulkanStaticWriteTexture::OnEditEnd(bool modified) {}

void VulkanStaticWriteTexture::DoRender(VulkanRenderState* state) {
  render_frame_ = state->frame;

  if (host_buffer_ == nullptr) {
    return;
  }

  int num_regions = static_cast<int>(dirty_regions_.size());
  for (int i = 0; i < num_regions; ++i) {
    UpdateImage(state, host_buffer_.get(), image_.get(), dirty_regions_[i],
                i == num_regions - 1);
  }
  host_buffer_.reset();
  dirty_regions_.clear();
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
                              return region.x >= x && region.y >= y &&
                                     region.x + region.width <= x + width &&
                                     region.y + region.height <= y + height;
                            });
  dirty_regions_.erase(end, dirty_regions_.end());
  dirty_regions_.emplace_back(x, y, width, height);
}

//==============================================================================
// VulkanStaticReadWriteTexture
//==============================================================================

class VulkanStaticReadWriteTexture final : public VulkanTexture {
 public:
  VulkanStaticReadWriteTexture(gb::ResourceEntry entry, VulkanBackend* backend,
                               vk::Sampler sampler, int width, int height,
                               const SamplerOptions& options,
                               TextureFormat format)
      : VulkanTexture(std::move(entry), backend, sampler,
                      DataVolatility::kStaticReadWrite, width, height, options,
                      format) {}

 protected:
  bool Init() override;
  bool DoClear(int x, int y, int width, int height, const void* texel) override;
  bool DoSet(int x, int y, int width, int height, const void* texels,
             int stride) override;
  bool DoGet(void* out_texels) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;

 private:
  ~VulkanStaticReadWriteTexture() override;

  bool EnsureHostBufferIsWritable(bool copy_contents);

  // Adds a region to upload. If the image is in use it is replaced, and every
  // pixel is uploaded to the new one.
  bool MarkDirty(int x, int y, int width, int height);

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
                                           const void* texel) {
  if (const bool copy_contents =
          x != 0 || y != 0 || width != GetWidth() || height != GetHeight();
      !EnsureHostBufferIsWritable(copy_contents)) {
    return false;
  }
  if (!MarkDirty(x, y, width, height)) {
    return false;
  }
  ClearRegion(host_buffer_->GetData(), x, y, width, height, texel);
  return true;
}

bool VulkanStaticReadWriteTexture::DoSet(int x, int y, int width, int height,
                                         const void* texels, int stride) {
  if (const bool copy_contents =
          x != 0 || y != 0 || width != GetWidth() || height != GetHeight();
      !EnsureHostBufferIsWritable(copy_contents)) {
    return false;
  }
  if (!MarkDirty(x, y, width, height)) {
    return false;
  }
  CopyRegion(host_buffer_->GetData(), x, y, width, height, texels, stride);
  return true;
}

bool VulkanStaticReadWriteTexture::DoGet(void* out_texels) {
  // The host buffer holds the texture first, then its mipmaps.
  std::memcpy(out_texels, host_buffer_->GetData(), GetSizeInBytes());
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
  MarkDirty(0, 0, GetWidth(), GetHeight());
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

bool VulkanStaticReadWriteTexture::MarkDirty(int x, int y, int width,
                                             int height) {
  if (dirty_) {
    // TODO: If performance warrants it, we could maintain a list of
    //       non-intersecting regions to update instead of just unioning all
    //       regions together.
    UnionDirtyRegion(&dirty_region_, x, y, width, height);
    return true;
  }

  if (FrameInUse(render_frame_)) {
    auto image = CreateImage();
    if (image == nullptr) {
      return false;
    }
    image_ = std::move(image);
    SetAllImageHandles(image_.get());

    // This image has never been rendered, and needs every pixel.
    render_frame_ = kNeverUsedFrame;
    x = 0;
    y = 0;
    width = GetWidth();
    height = GetHeight();
  }
  dirty_ = true;
  dirty_region_ = {x, y, width, height};
  return true;
}

//==============================================================================
// VulkanPerFrameTexture
//==============================================================================

class VulkanPerFrameTexture final : public VulkanTexture {
 public:
  VulkanPerFrameTexture(gb::ResourceEntry entry, VulkanBackend* backend,
                        vk::Sampler sampler, int width, int height,
                        const SamplerOptions& options, TextureFormat format)
      : VulkanTexture(std::move(entry), backend, sampler,
                      DataVolatility::kPerFrame, width, height, options,
                      format) {}

 protected:
  bool Init() override;
  bool DoClear(int x, int y, int width, int height, const void* texel) override;
  bool DoSet(int x, int y, int width, int height, const void* texels,
             int stride) override;
  bool DoGet(void* out_texels) override;
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
  local_buffer_ = std::calloc(GetSizeInBytes(), 1);
  if (local_buffer_ == nullptr) {
    return false;
  }
  for (int i = 0; i < kMaxFramesInFlight; ++i) {
    SetImageHandle(i, frame_data_[i].image.get());
  }
  return true;
}

bool VulkanPerFrameTexture::DoClear(int x, int y, int width, int height,
                                    const void* texel) {
  ClearRegion(local_buffer_, x, y, width, height, texel);
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
                                  const void* texels, int stride) {
  CopyRegion(local_buffer_, x, y, width, height, texels, stride);
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

bool VulkanPerFrameTexture::DoGet(void* out_texels) {
  std::memcpy(out_texels, local_buffer_, GetSizeInBytes());
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
             static_cast<const uint8_t*>(local_buffer_) +
                 GetRegionOffset(region.x, region.y),
             GetWidth());
  UpdateImage(state, frame_data.host.get(), frame_data.image.get(), region);
  frame_data.dirty_region = {};
  frame_data.dirty = false;
}

}  // namespace

VulkanTexture::VulkanTexture(gb::ResourceEntry entry, VulkanBackend* backend,
                             vk::Sampler sampler, DataVolatility volatility,
                             int width, int height,
                             const SamplerOptions& options,
                             TextureFormat format)
    : Texture(std::move(entry), volatility, width, height, options, format),
      backend_(backend),
      sampler_(sampler),
      mip_levels_(GetMipLevelCount(width, height, options)),
      host_size_(GetMipChainSizeInBytes(format, width, height, mip_levels_)) {}

VulkanTexture::~VulkanTexture() {}

VulkanTexture* VulkanTexture::Create(gb::ResourceEntry entry,
                                     VulkanBackend* backend,
                                     vk::Sampler sampler,
                                     DataVolatility volatility, int width,
                                     int height, const SamplerOptions& options,
                                     TextureFormat format) {
  VulkanTexture* texture = nullptr;
  switch (volatility) {
    case DataVolatility::kStaticWrite:
      texture = new VulkanStaticWriteTexture(std::move(entry), backend, sampler,
                                             width, height, options, format);
      break;
    case DataVolatility::kStaticReadWrite:
      texture = new VulkanStaticReadWriteTexture(
          std::move(entry), backend, sampler, width, height, options, format);
      break;
    case DataVolatility::kPerFrame:
      texture = new VulkanPerFrameTexture(std::move(entry), backend, sampler,
                                          width, height, options, format);
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
  const VulkanTextureFormat format = GetVulkanTextureFormat(GetFormat());
  return VulkanImage::Create(
      backend_, GetWidth(), GetHeight(), 1, format.format,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      VulkanImage::Options()
          .SetMipLevels(mip_levels_)
          .SetComponents(format.components));
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
      host_buffer->Get(),
      static_cast<uint32_t>(GetRegionOffset(region.x, region.y)), image->Get(),
      /*mip_level=*/0, GetWidth(), GetHeight(),
      /*image_layer=*/0, region.x, region.y, region.width, region.height);
  if (update_mips) {
    // A texture updated in several regions has none that covers the whole
    // texture, as that region would contain the others.
    const bool keep_contents = region.x != 0 || region.y != 0 ||
                               region.width != GetWidth() ||
                               region.height != GetHeight();
    state->image_barriers.emplace_back(image, mip_levels_, /*layer=*/0,
                                       keep_contents);
  }
  if (!update_mips || mip_levels_ == 1) {
    return;
  }

  GenerateMipmaps(GetFormat(), GetWidth(), GetHeight(), mip_levels_,
                  GetSamplerOptions().tile_size, host_buffer->GetData());
  for (int mip = 1; mip < mip_levels_; ++mip) {
    state->image_updates.emplace_back(
        host_buffer->Get(),
        static_cast<uint32_t>(
            GetMipLevelOffset(GetFormat(), GetWidth(), GetHeight(), mip)),
        image->Get(), mip, GetWidth() >> mip, GetHeight() >> mip);
  }
}

void VulkanTexture::ClearRegion(void* buffer, int x, int y, int width,
                                int height, const void* texel) {
  const int row_size = GetWidth() * GetTexelSize(GetFormat());
  uint8_t* dst = static_cast<uint8_t*>(buffer) + GetRegionOffset(x, y);
  for (int row = 0; row < height; ++row) {
    FillTexels(GetFormat(), texel, width, dst);
    dst += row_size;
  }
}

void VulkanTexture::CopyRegion(void* buffer, int x, int y, int width,
                               int height, const void* texels, int stride) {
  uint8_t* dst = static_cast<uint8_t*>(buffer) + GetRegionOffset(x, y);
  const uint8_t* src = static_cast<const uint8_t*>(texels);
  if (stride == width && width == GetWidth()) {
    std::memcpy(dst, src, GetTextureSizeInBytes(GetFormat(), width, height));
    return;
  }
  const int texel_size = GetTexelSize(GetFormat());
  const int dst_row_size = GetWidth() * texel_size;
  const int src_row_size = stride * texel_size;
  for (int row = 0; row < height; ++row) {
    std::memcpy(dst, src, width * texel_size);
    dst += dst_row_size;
    src += src_row_size;
  }
}

}  // namespace gb
