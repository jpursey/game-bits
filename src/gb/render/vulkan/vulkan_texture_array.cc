// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_texture_array.h"

#include <cstdint>
#include <cstring>
#include <memory>

#include "absl/log/log.h"
#include "gb/render/mipmaps.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_buffer.h"
#include "gb/render/vulkan/vulkan_format.h"

namespace gb {

VulkanTextureArray* VulkanTextureArray::Create(
    gb::ResourceEntry entry, VulkanBackend* backend, vk::Sampler sampler,
    DataVolatility volatility, int count, int width, int height,
    const SamplerOptions& options, TextureFormat format) {
  VulkanTextureArray* texture_array = nullptr;
  if (volatility != DataVolatility::kStaticWrite &&
      volatility != DataVolatility::kStaticReadWrite) {
    LOG(ERROR) << "Unhandled data volatility for texture array";
    return nullptr;
  }
  texture_array =
      new VulkanTextureArray(std::move(entry), backend, sampler, volatility,
                             count, width, height, options, format);
  texture_array->host_buffer_ = texture_array->CreateHostBuffer();
  if (texture_array->host_buffer_ == nullptr || !texture_array->CreateImage()) {
    texture_array->Delete();
    return nullptr;
  }
  return texture_array;
}

VulkanTextureArray::VulkanTextureArray(
    gb::ResourceEntry entry, VulkanBackend* backend, vk::Sampler sampler,
    DataVolatility volatility, int count, int width, int height,
    const SamplerOptions& options, TextureFormat format)
    : TextureArray(std::move(entry), volatility, count, width, height, options,
                   format),
      backend_(backend),
      sampler_(sampler),
      mip_levels_(GetMipLevelCount(width, height, options)),
      host_layer_size_(
          GetMipChainSizeInBytes(format, width, height, mip_levels_)) {}

VulkanTextureArray::~VulkanTextureArray() {}

bool VulkanTextureArray::CreateImage() {
  const VulkanTextureFormat format = GetVulkanTextureFormat(GetFormat());
  auto image = VulkanImage::Create(
      backend_, GetWidth(), GetHeight(), GetCount(), format.format,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      VulkanImage::Options()
          .SetMipLevels(mip_levels_)
          .SetViewType(vk::ImageViewType::e2DArray)
          .SetComponents(format.components));
  if (image == nullptr) {
    return false;
  }
  image_ = std::move(image);
  image_handle_.version += 1;
  image_handle_.image = image_.get();
  return true;
}

std::unique_ptr<VulkanBuffer> VulkanTextureArray::CreateHostBuffer() {
  return VulkanBuffer::Create(
      backend_,
      vk::BufferCreateInfo()
          .setSize(host_layer_size_ * GetCount())
          .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
          .setSharingMode(vk::SharingMode::eExclusive),
      VMA_MEMORY_USAGE_CPU_ONLY);
}

bool VulkanTextureArray::EnsureHostBufferIsWritable() {
  if (host_buffer_ == nullptr) {
    host_buffer_ = CreateHostBuffer();
    return host_buffer_ != nullptr;
  }
  if (!FrameInUse(transfer_frame_)) {
    return true;
  }

  // The host buffer may still be copied from, so its contents move to a new
  // one.
  auto host_buffer = CreateHostBuffer();
  if (host_buffer == nullptr) {
    return false;
  }
  std::memcpy(host_buffer->GetData(), host_buffer_->GetData(),
              host_buffer_->GetSize());
  host_buffer_ = std::move(host_buffer);

  // This host buffer has never been transferred.
  transfer_frame_ = kNeverUsedFrame;
  return true;
}

void VulkanTextureArray::UploadTexture(VulkanRenderState* state, int index) {
  state->image_barriers.emplace_back(image_.get(), mip_levels_, index,
                                     /*keep_contents=*/false);
  for (int mip = 0; mip < mip_levels_; ++mip) {
    const int64_t offset =
        index * host_layer_size_ +
        GetMipLevelOffset(GetFormat(), GetWidth(), GetHeight(), mip);
    state->image_updates.emplace_back(
        host_buffer_->Get(), static_cast<uint32_t>(offset), image_->Get(), mip,
        GetWidth() >> mip, GetHeight() >> mip, index);
  }
}

uint8_t* VulkanTextureArray::GetHostData(int index) {
  return static_cast<uint8_t*>(host_buffer_->GetData()) +
         (host_layer_size_ * index);
}

uint8_t* VulkanTextureArray::ModifyHostData(int index) {
  // An image in use is replaced by a new one, which needs every texture. The
  // textures of a kStaticWrite array can't be read back, so unless there is
  // only the one being modified, it can't be replaced.
  const bool image_in_use = FrameInUse(render_frame_);
  if (image_in_use && GetVolatility() == DataVolatility::kStaticWrite &&
      GetCount() > 1) {
    return nullptr;
  }

  if (!EnsureHostBufferIsWritable()) {
    return nullptr;
  }

  if (image_in_use) {
    if (!CreateImage()) {
      return nullptr;
    }
    render_frame_ = kNeverUsedFrame;
    upload_all_ = true;
  }

  return GetHostData(index);
}

bool VulkanTextureArray::DoClear(int index, const void* texel) {
  uint8_t* host_data = ModifyHostData(index);
  if (host_data == nullptr) {
    return false;
  }

  FillTexels(GetFormat(), texel, GetWidth() * GetHeight(), host_data);
  updates_.insert(index);
  return true;
}

bool VulkanTextureArray::DoSet(int index, const void* texels) {
  uint8_t* host_data = ModifyHostData(index);
  if (host_data == nullptr) {
    return false;
  }

  std::memcpy(host_data, texels, GetLayerSizeInBytes());
  updates_.insert(index);
  return true;
}

bool VulkanTextureArray::DoGet(int index, void* out_texels) {
  if (GetVolatility() == DataVolatility::kStaticWrite) {
    return false;
  }

  // The host data for each texture is followed by its mipmaps, which aren't
  // returned.
  std::memcpy(out_texels, GetHostData(index), GetLayerSizeInBytes());
  return true;
}

void VulkanTextureArray::OnRender(VulkanRenderState* state) {
  render_frame_ = state->frame;
  if (updates_.empty() && !upload_all_) {
    return;
  }

  // The mipmaps of textures that weren't modified are already in the host
  // buffer.
  for (int index : updates_) {
    GenerateMipmaps(GetFormat(), GetWidth(), GetHeight(), mip_levels_,
                    GetSamplerOptions().tile_size, GetHostData(index));
  }
  if (upload_all_) {
    for (int index = 0; index < GetCount(); ++index) {
      UploadTexture(state, index);
    }
  } else {
    for (int index : updates_) {
      UploadTexture(state, index);
    }
  }
  updates_.clear();
  upload_all_ = false;
  transfer_frame_ = render_frame_;

  if (GetVolatility() == DataVolatility::kStaticWrite) {
    host_buffer_.reset();
  }
}

}  // namespace gb
