// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_texture_array.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

#include "absl/log/log.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_buffer.h"
#include "stb_image_resize.h"

namespace gb {

VulkanTextureArray* VulkanTextureArray::Create(gb::ResourceEntry entry,
                                               VulkanBackend* backend,
                                               vk::Sampler sampler,
                                               DataVolatility volatility,
                                               int count, int width, int height,
                                               const SamplerOptions& options) {
  VulkanTextureArray* texture_array = nullptr;
  if (volatility != DataVolatility::kStaticWrite &&
      volatility != DataVolatility::kStaticReadWrite) {
    LOG(ERROR) << "Unhandled data volatility for texture array";
    return nullptr;
  }
  texture_array =
      new VulkanTextureArray(std::move(entry), backend, sampler, volatility,
                             count, width, height, options);
  texture_array->host_buffer_ = texture_array->CreateHostBuffer();
  if (texture_array->host_buffer_ == nullptr || !texture_array->CreateImage()) {
    texture_array->Delete();
    return nullptr;
  }
  return texture_array;
}

VulkanTextureArray::VulkanTextureArray(gb::ResourceEntry entry,
                                       VulkanBackend* backend,
                                       vk::Sampler sampler,
                                       DataVolatility volatility, int count,
                                       int width, int height,
                                       const SamplerOptions& options)
    : TextureArray(std::move(entry), volatility, count, width, height, options),
      backend_(backend),
      sampler_(sampler) {
  host_layer_size_ = width * height * sizeof(Pixel);
  if (options.mipmap) {
    int size = options.tile_size;
    if (size == 0) {
      size = std::min(GetWidth(), GetHeight());
    }
    size >>= 1;
    while (size != 0) {
      host_layer_size_ +=
          (width >> mip_levels_) * (height >> mip_levels_) * sizeof(Pixel);
      size >>= 1;
      ++mip_levels_;
    }
  }
}

VulkanTextureArray::~VulkanTextureArray() {}

bool VulkanTextureArray::CreateImage() {
  auto image = VulkanImage::Create(
      backend_, GetWidth(), GetHeight(), GetCount(), vk::Format::eR8G8B8A8Srgb,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      VulkanImage::Options()
          .SetMipLevels(mip_levels_)
          .SetViewType(vk::ImageViewType::e2DArray));
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
  uint32_t offset = index * host_layer_size_;
  int width = GetWidth();
  int height = GetHeight();
  for (int mip = 0; mip < mip_levels_; ++mip) {
    state->image_updates.emplace_back(host_buffer_->Get(), offset,
                                      image_->Get(), mip, width, height, index);
    offset += width * height * sizeof(Pixel);
    width >>= 1;
    height >>= 1;
  }
}

void VulkanTextureArray::GenerateMipmaps(int index) {
  int src_tile_size = GetSamplerOptions().tile_size;
  int src_width = GetWidth();
  int src_height = GetHeight();
  unsigned char* src = static_cast<unsigned char*>(host_buffer_->GetData()) +
                       index * host_layer_size_;
  unsigned char* dst = src + (src_width * src_height * sizeof(Pixel));
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

    src = dst;
    src_width = dst_width;
    src_height = dst_height;
    dst += mip_byte_size;
  }
}

void* VulkanTextureArray::ModifyHostData(int index) {
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

  return static_cast<uint8_t*>(host_buffer_->GetData()) +
         (host_layer_size_ * index);
}

bool VulkanTextureArray::DoClear(int index, Pixel pixel) {
  void* host_data = ModifyHostData(index);
  if (host_data == nullptr) {
    return false;
  }

  if (pixel.Packed() == 0) {
    std::memset(host_data, 0, host_layer_size_);
  } else {
    const int width = GetWidth();
    const int height = GetHeight();
    Pixel* dst = static_cast<Pixel*>(host_data);
    for (int i = 0; i < width * height; ++i) {
      dst[i] = pixel;
    }
  }
  updates_.insert(index);
  return true;
}

bool VulkanTextureArray::DoSet(int index, const void* pixels) {
  void* host_data = ModifyHostData(index);
  if (host_data == nullptr) {
    return false;
  }

  std::memcpy(host_data, pixels, GetWidth() * GetHeight() * sizeof(Pixel));
  updates_.insert(index);
  return true;
}

bool VulkanTextureArray::DoGet(int index, void* out_pixels) {
  if (GetVolatility() == DataVolatility::kStaticWrite) {
    return false;
  }

  // The host data for each texture is followed by its mipmaps, which aren't
  // returned.
  const void* host_data = static_cast<uint8_t*>(host_buffer_->GetData()) +
                          (host_layer_size_ * index);
  std::memcpy(out_pixels, host_data, GetWidth() * GetHeight() * sizeof(Pixel));
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
    GenerateMipmaps(index);
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
