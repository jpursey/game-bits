// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_RENDER_TARGET_H_
#define GB_RENDER_VULKAN_VULKAN_RENDER_TARGET_H_

#include <memory>

#include "gb/render/render_target.h"
#include "gb/render/render_types.h"
#include "gb/render/sampler_options.h"
#include "gb/render/vulkan/vulkan_image.h"
#include "gb/render/vulkan/vulkan_render_state.h"
#include "gb/render/vulkan/vulkan_texels.h"
#include "gb/render/vulkan/vulkan_types.h"
#include "gb/resource/resource_entry.h"

namespace gb {

// Vulkan implementation of RenderTarget.
//
// Its image is in eShaderReadOnlyOptimal layout outside the passes that draw
// into it. A new image is cleared into that layout at the start of the first
// frame that uses it, and each pass leaves it there.
//
// This class is thread-compatible.
class VulkanRenderTarget final : public RenderTarget {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a target whose image is in `vulkan_format`, the Vulkan format for
  // `format`. Returns null if the image can't be created.
  //
  // This is thread-safe.
  static VulkanRenderTarget* Create(ResourceEntry entry, VulkanBackend* backend,
                                    vk::Sampler sampler, TextureFormat format,
                                    vk::Format vulkan_format,
                                    const RenderTargetSize& size, int width,
                                    int height, const SamplerOptions& options);

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // The image to draw into and sample. The version changes whenever the image
  // is replaced, as it is when the target is resized.
  const VulkanTexels::ImageHandle& GetImageHandle() const {
    return image_handle_;
  }
  vk::Sampler GetSampler() const { return sampler_; }

  //----------------------------------------------------------------------------
  // Events
  //----------------------------------------------------------------------------

  // Queues the clear of a new image, which the frame must do before drawing.
  void OnRender(VulkanRenderState* state);

 protected:
  bool DoResize(int width, int height) override;

 private:
  VulkanRenderTarget(ResourceEntry entry, VulkanBackend* backend,
                     vk::Sampler sampler, TextureFormat format,
                     const RenderTargetSize& size, int width, int height,
                     const SamplerOptions& options,
                     std::unique_ptr<VulkanImage> image);
  ~VulkanRenderTarget() override = default;

  // Replaces the image with a new one, to be cleared.
  void SetImage(std::unique_ptr<VulkanImage> image);

  VulkanBackend* const backend_;
  const vk::Sampler sampler_;
  std::unique_ptr<VulkanImage> image_;
  VulkanTexels::ImageHandle image_handle_ = {};
  bool cleared_ = false;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_RENDER_TARGET_H_
