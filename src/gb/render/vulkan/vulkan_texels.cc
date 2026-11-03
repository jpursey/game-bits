// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_texels.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "absl/log/log.h"
#include "gb/render/mipmaps.h"
#include "gb/render/vulkan/vulkan_format.h"

namespace gb {

namespace {

using Region = VulkanTexels::Region;

// Returns the smallest region containing both a and b.
Region UnionRegions(const Region& a, const Region& b) {
  const int x1 = std::min(a.x, b.x);
  const int y1 = std::min(a.y, b.y);
  const int z1 = std::min(a.z, b.z);
  const int x2 = std::max(a.x + a.width, b.x + b.width);
  const int y2 = std::max(a.y + a.height, b.y + b.height);
  const int z2 = std::max(a.z + a.depth, b.z + b.depth);
  return {x1, y1, z1, x2 - x1, y2 - y1, z2 - z1};
}

//==============================================================================
// VulkanStaticWriteTexels
//==============================================================================

class VulkanStaticWriteTexels final : public VulkanTexels {
 public:
  using VulkanTexels::VulkanTexels;

 protected:
  bool Init() override;
  void* PrepareWrite(const Region& region) override;
  bool DoGet(void* out_texels) const override;
  void* DoEditBegin() override;
  void DoEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;

 private:
  void AddDirtyRegion(const Region& region);

  absl::InlinedVector<Region, 1> dirty_regions_;
  int render_frame_ = kNeverUsedFrame;
  std::unique_ptr<VulkanImage> image_;

  // Holds the texels written since the last upload, and is null if there are
  // none.
  std::unique_ptr<VulkanBuffer> host_buffer_;
};

bool VulkanStaticWriteTexels::Init() {
  image_ = CreateImage();
  if (image_ == nullptr) {
    return false;
  }
  SetAllImageHandles(image_.get());
  return true;
}

void* VulkanStaticWriteTexels::PrepareWrite(const Region& region) {
  if (host_buffer_ == nullptr) {
    // The texels can't be read back, so a partial update can't be applied to a
    // new image (which replaces one in use), or used to generate mipmaps from.
    const bool in_use = FrameInUse(render_frame_);
    if (!IsWholeRegion(region) && (in_use || GetMipLevels() > 1)) {
      return nullptr;
    }

    host_buffer_ = CreateHostBuffer();
    if (host_buffer_ == nullptr) {
      return nullptr;
    }

    if (in_use) {
      auto image = CreateImage();
      if (image == nullptr) {
        host_buffer_.reset();
        return nullptr;
      }
      image_ = std::move(image);
      SetAllImageHandles(image_.get());
      render_frame_ = kNeverUsedFrame;
    }
  }
  AddDirtyRegion(region);
  return host_buffer_->GetData();
}

bool VulkanStaticWriteTexels::DoGet(void* out_texels) const { return false; }
void* VulkanStaticWriteTexels::DoEditBegin() { return nullptr; }
void VulkanStaticWriteTexels::DoEditEnd(bool modified) {}

void VulkanStaticWriteTexels::DoRender(VulkanRenderState* state) {
  render_frame_ = state->frame;

  if (host_buffer_ == nullptr) {
    return;
  }

  const int num_regions = static_cast<int>(dirty_regions_.size());
  for (int i = 0; i < num_regions; ++i) {
    UpdateImage(state, host_buffer_.get(), image_.get(), dirty_regions_[i],
                /*last=*/i == num_regions - 1);
  }
  host_buffer_.reset();
  dirty_regions_.clear();
}

void VulkanStaticWriteTexels::AddDirtyRegion(const Region& region) {
  // Skip this region if it is fully contained in an existing region.
  for (const Region& dirty_region : dirty_regions_) {
    if (dirty_region.Contains(region)) {
      return;
    }
  }

  // Remove any regions fully contained in this region.
  auto end = std::remove_if(dirty_regions_.begin(), dirty_regions_.end(),
                            [&region](const Region& dirty_region) {
                              return region.Contains(dirty_region);
                            });
  dirty_regions_.erase(end, dirty_regions_.end());
  dirty_regions_.push_back(region);
}

//==============================================================================
// VulkanStaticReadWriteTexels
//==============================================================================

class VulkanStaticReadWriteTexels final : public VulkanTexels {
 public:
  using VulkanTexels::VulkanTexels;

 protected:
  bool Init() override;
  void* PrepareWrite(const Region& region) override;
  bool DoGet(void* out_texels) const override;
  void* DoEditBegin() override;
  void DoEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;

 private:
  bool EnsureHostBufferIsWritable(bool copy_contents);

  // Adds a region to upload. If the image is in use it is replaced, and every
  // texel is uploaded to the new one.
  bool MarkDirty(const Region& region);

  // The region to upload, if any, and while editing, the region there was to
  // upload before the edit began.
  std::optional<Region> dirty_region_;
  std::optional<Region> dirty_region_before_edit_;

  int render_frame_ = kNeverUsedFrame;
  int transfer_frame_ = kNeverUsedFrame;
  std::unique_ptr<VulkanImage> image_;
  std::unique_ptr<VulkanBuffer> host_buffer_;
};

bool VulkanStaticReadWriteTexels::Init() {
  image_ = CreateImage();
  host_buffer_ = CreateHostBuffer();
  if (image_ == nullptr || host_buffer_ == nullptr) {
    return false;
  }
  SetAllImageHandles(image_.get());
  return true;
}

void* VulkanStaticReadWriteTexels::PrepareWrite(const Region& region) {
  // Marking it dirty first means a failure leaves the host buffer as it was,
  // rather than a new one that has none of the texels.
  if (!MarkDirty(region) ||
      !EnsureHostBufferIsWritable(/*copy_contents=*/!IsWholeRegion(region))) {
    return nullptr;
  }
  return host_buffer_->GetData();
}

bool VulkanStaticReadWriteTexels::DoGet(void* out_texels) const {
  // The host buffer holds the texels first, then their mipmaps.
  std::memcpy(out_texels, host_buffer_->GetData(), GetSizeInBytes());
  return true;
}

void* VulkanStaticReadWriteTexels::DoEditBegin() {
  if (!EnsureHostBufferIsWritable(true)) {
    return nullptr;
  }

  // Nothing is uploaded until DoEditEnd is called.
  dirty_region_before_edit_ = std::exchange(dirty_region_, std::nullopt);
  return host_buffer_->GetData();
}

void VulkanStaticReadWriteTexels::DoEditEnd(bool modified) {
  if (!modified) {
    dirty_region_ = dirty_region_before_edit_;
    return;
  }
  MarkDirty(GetWholeRegion());
}

void VulkanStaticReadWriteTexels::DoRender(VulkanRenderState* state) {
  render_frame_ = state->frame;

  if (!dirty_region_.has_value()) {
    return;
  }

  UpdateImage(state, host_buffer_.get(), image_.get(), *dirty_region_,
              /*last=*/true);
  transfer_frame_ = render_frame_;
  dirty_region_.reset();
}

bool VulkanStaticReadWriteTexels::EnsureHostBufferIsWritable(
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

bool VulkanStaticReadWriteTexels::MarkDirty(const Region& region) {
  if (dirty_region_.has_value()) {
    // TODO: If performance warrants it, we could maintain a list of
    //       non-intersecting regions to update instead of just unioning all
    //       regions together.
    dirty_region_ = UnionRegions(*dirty_region_, region);
    return true;
  }

  Region dirty_region = region;
  if (FrameInUse(render_frame_)) {
    auto image = CreateImage();
    if (image == nullptr) {
      return false;
    }
    image_ = std::move(image);
    SetAllImageHandles(image_.get());

    // This image has never been rendered, and needs every texel.
    render_frame_ = kNeverUsedFrame;
    dirty_region = GetWholeRegion();
  }
  dirty_region_ = dirty_region;
  return true;
}

//==============================================================================
// VulkanPerFrameTexels
//==============================================================================

class VulkanPerFrameTexels final : public VulkanTexels {
 public:
  using VulkanTexels::VulkanTexels;
  ~VulkanPerFrameTexels() override { std::free(local_buffer_); }

 protected:
  bool Init() override;
  void* PrepareWrite(const Region& region) override;
  bool DoGet(void* out_texels) const override;
  void* DoEditBegin() override;
  void DoEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;

 private:
  struct FrameData {
    // The region to upload to this frame's image, if any.
    std::optional<Region> dirty_region;

    std::unique_ptr<VulkanBuffer> host;
    std::unique_ptr<VulkanImage> image;
  };

  // Holds the texels, without mipmaps, laid out as a host buffer is.
  void* local_buffer_ = nullptr;

  FrameData frame_data_[kMaxFramesInFlight];
};

bool VulkanPerFrameTexels::Init() {
  for (FrameData& frame_data : frame_data_) {
    frame_data.host = CreateHostBuffer();
    frame_data.image = CreateImage();
    if (frame_data.host == nullptr || frame_data.image == nullptr) {
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

void* VulkanPerFrameTexels::PrepareWrite(const Region& region) {
  for (FrameData& frame_data : frame_data_) {
    frame_data.dirty_region =
        frame_data.dirty_region.has_value()
            ? UnionRegions(*frame_data.dirty_region, region)
            : region;
  }
  return local_buffer_;
}

bool VulkanPerFrameTexels::DoGet(void* out_texels) const {
  std::memcpy(out_texels, local_buffer_, GetSizeInBytes());
  return true;
}

void* VulkanPerFrameTexels::DoEditBegin() { return local_buffer_; }

void VulkanPerFrameTexels::DoEditEnd(bool modified) {
  if (modified) {
    for (FrameData& frame_data : frame_data_) {
      frame_data.dirty_region = GetWholeRegion();
    }
  }
}

void VulkanPerFrameTexels::DoRender(VulkanRenderState* state) {
  FrameData& frame_data = frame_data_[state->frame % kMaxFramesInFlight];
  if (!frame_data.dirty_region.has_value()) {
    return;
  }

  CopyHostRegion(frame_data.host->GetData(), *frame_data.dirty_region,
                 local_buffer_);
  UpdateImage(state, frame_data.host.get(), frame_data.image.get(),
              *frame_data.dirty_region, /*last=*/true);
  frame_data.dirty_region.reset();
}

}  // namespace

//==============================================================================
// VulkanTexels
//==============================================================================

std::unique_ptr<VulkanTexels> VulkanTexels::CreateTexture(
    VulkanBackend* backend, DataVolatility volatility, TextureFormat format,
    int width, int height, const SamplerOptions& options) {
  const Properties properties = {
      .format = format,
      .width = width,
      .height = height,
      .mip_levels = GetMipLevelCount(width, height, options),
      .tile_size = options.tile_size,
  };
  std::unique_ptr<VulkanTexels> texels;
  switch (volatility) {
    case DataVolatility::kStaticWrite:
      texels.reset(new VulkanStaticWriteTexels(backend, properties));
      break;
    case DataVolatility::kStaticReadWrite:
      texels.reset(new VulkanStaticReadWriteTexels(backend, properties));
      break;
    case DataVolatility::kPerFrame:
      texels.reset(new VulkanPerFrameTexels(backend, properties));
      break;
    default:
      LOG(ERROR) << "Unhandled data volatility for texture";
      return nullptr;
  }
  if (!texels->Init()) {
    return nullptr;
  }
  return texels;
}

VulkanTexels::VulkanTexels(VulkanBackend* backend, const Properties& properties)
    : backend_(backend),
      format_(properties.format),
      width_(properties.width),
      height_(properties.height),
      depth_(properties.depth),
      view_type_(properties.view_type),
      mip_levels_(properties.mip_levels),
      tile_size_(properties.tile_size),
      // Only a 2D texture has mipmaps, so the mip chain has one slice.
      host_size_(GetMipChainSizeInBytes(format_, width_, height_, mip_levels_) *
                 depth_) {}

bool VulkanTexels::Clear(const Region& region, const void* texel) {
  uint8_t* const buffer = static_cast<uint8_t*>(PrepareWrite(region));
  if (buffer == nullptr) {
    return false;
  }

  const int64_t row_size = GetTextureSizeInBytes(format_, width_, 1);
  const int64_t slice_size = GetTextureSizeInBytes(format_, width_, height_);
  uint8_t* slice = buffer + GetRegionOffset(region);
  for (int z = 0; z < region.depth; ++z) {
    if (region.width == width_) {
      FillTexels(format_, texel, region.width * region.height, slice);
    } else {
      uint8_t* row = slice;
      for (int y = 0; y < region.height; ++y) {
        FillTexels(format_, texel, region.width, row);
        row += row_size;
      }
    }
    slice += slice_size;
  }
  return true;
}

bool VulkanTexels::Set(const Region& region, const void* texels, int stride,
                       int slice_height) {
  void* const buffer = PrepareWrite(region);
  if (buffer == nullptr) {
    return false;
  }
  const int64_t src_row_size = GetTextureSizeInBytes(format_, stride, 1);
  CopyRegion(buffer, region, texels, src_row_size, src_row_size * slice_height);
  return true;
}

int64_t VulkanTexels::GetRegionOffset(const Region& region) const {
  return ((static_cast<int64_t>(region.z) * height_ + region.y) * width_ +
          region.x) *
         GetTexelSize(format_);
}

std::unique_ptr<VulkanBuffer> VulkanTexels::CreateHostBuffer() {
  return VulkanBuffer::Create(
      backend_,
      vk::BufferCreateInfo()
          .setSize(host_size_)
          .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
          .setSharingMode(vk::SharingMode::eExclusive),
      VMA_MEMORY_USAGE_CPU_ONLY);
}

std::unique_ptr<VulkanImage> VulkanTexels::CreateImage() {
  const VulkanTextureFormat format = GetVulkanTextureFormat(format_);
  return VulkanImage::Create(
      backend_, width_, height_, 1, format.format,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      VulkanImage::Options()
          .SetViewType(view_type_)
          .SetDepth(depth_)
          .SetMipLevels(mip_levels_)
          .SetComponents(format.components));
}

void VulkanTexels::CopyHostRegion(void* buffer, const Region& region,
                                  const void* src_texels) const {
  CopyRegion(buffer, region,
             static_cast<const uint8_t*>(src_texels) + GetRegionOffset(region),
             GetTextureSizeInBytes(format_, width_, 1),
             GetTextureSizeInBytes(format_, width_, height_));
}

void VulkanTexels::CopyRegion(void* buffer, const Region& region,
                              const void* src, int64_t src_row_size,
                              int64_t src_slice_size) const {
  const int64_t row_size = GetTextureSizeInBytes(format_, width_, 1);
  const int64_t slice_size = GetTextureSizeInBytes(format_, width_, height_);
  const int64_t region_row_size =
      GetTextureSizeInBytes(format_, region.width, 1);

  // Rows as wide as the texture, and as far apart in src, copy a slice at a
  // time.
  const bool whole_rows =
      region_row_size == row_size && src_row_size == row_size;
  uint8_t* dst_slice = static_cast<uint8_t*>(buffer) + GetRegionOffset(region);
  const uint8_t* src_slice = static_cast<const uint8_t*>(src);
  for (int z = 0; z < region.depth; ++z) {
    if (whole_rows) {
      std::memcpy(dst_slice, src_slice, row_size * region.height);
    } else {
      uint8_t* dst_row = dst_slice;
      const uint8_t* src_row = src_slice;
      for (int y = 0; y < region.height; ++y) {
        std::memcpy(dst_row, src_row, region_row_size);
        dst_row += row_size;
        src_row += src_row_size;
      }
    }
    dst_slice += slice_size;
    src_slice += src_slice_size;
  }
}

void VulkanTexels::SetAllImageHandles(VulkanImage* image) {
  for (ImageHandle& handle : image_handles_) {
    handle.version += 1;
    handle.image = image;
  }
}

void VulkanTexels::SetImageHandle(int index, VulkanImage* image) {
  ImageHandle& handle = image_handles_[index];
  handle.version += 1;
  handle.image = image;
}

void VulkanTexels::UpdateImage(VulkanRenderState* state,
                               VulkanBuffer* host_buffer, VulkanImage* image,
                               const Region& region, bool last) {
  state->image_updates.emplace_back(
      host_buffer->Get(), static_cast<uint32_t>(GetRegionOffset(region)),
      image->Get(), /*mip_level=*/0, width_, height_, /*image_layer=*/0,
      vk::Offset3D(region.x, region.y, region.z),
      vk::Extent3D(region.width, region.height, region.depth));
  if (!last) {
    return;
  }

  // An image updated in several regions has none that covers the whole image,
  // as that region would contain the others.
  state->image_barriers.emplace_back(image, mip_levels_, /*layer=*/0,
                                     /*keep_contents=*/!IsWholeRegion(region));
  if (mip_levels_ == 1) {
    return;
  }

  GenerateMipmaps(format_, width_, height_, mip_levels_, tile_size_,
                  host_buffer->GetData());
  for (int mip = 1; mip < mip_levels_; ++mip) {
    state->image_updates.emplace_back(
        host_buffer->Get(),
        static_cast<uint32_t>(GetMipLevelOffset(format_, width_, height_, mip)),
        image->Get(), mip, width_ >> mip, height_ >> mip);
  }
}

}  // namespace gb
