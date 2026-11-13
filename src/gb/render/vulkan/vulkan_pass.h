// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_PASS_H_
#define GB_RENDER_VULKAN_VULKAN_PASS_H_

#include <memory>

#include "absl/container/inlined_vector.h"
#include "gb/render/render_pass.h"
#include "gb/render/vulkan/vulkan_render_state.h"
#include "gb/render/vulkan/vulkan_render_target.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Vulkan implementation of RenderPass: a render pass with an attachment for
// each target, and a framebuffer of the targets' images.
//
// Each target is in eShaderReadOnlyOptimal layout as the pass begins and ends,
// ready to sample. The render pass's dependencies order its writes after
// earlier shader reads and attachment writes, and before later shader reads.
// Every frame is submitted to the one queue, so this orders them with other
// frames in flight too.
//
// This class is thread-compatible.
class VulkanPass final : public RenderPass {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // The config is valid (see RenderSystem::CreatePass). Returns null if the
  // render pass can't be created, or the pass is multisampled, which isn't
  // supported yet.
  static std::unique_ptr<VulkanPass> Create(VulkanBackend* backend,
                                            const RenderPassConfig& config,
                                            int pass_order);

  ~VulkanPass() override;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  const VulkanRenderPassInfo& GetRenderPass() const { return render_pass_; }

  // Returns the size of the targets' images, which the pass draws into.
  vk::Extent2D GetExtent() const;

  // Returns the values to clear each attachment to, in order.
  absl::InlinedVector<vk::ClearValue, 2> GetClearValues() const;

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Returns the framebuffer of the targets' images, recreating it if any image
  // has been replaced since. Returns null if it can't be created, which is
  // logged (at most once every 100 frames) and retried the next frame.
  vk::Framebuffer GetFramebuffer();

  //----------------------------------------------------------------------------
  // Events
  //----------------------------------------------------------------------------

  // Prepares the targets for the frame, which draws the pass whether or not
  // anything samples them.
  void OnRender(VulkanRenderState* state);

 private:
  // A target the pass draws into, in the render pass's attachment order: color
  // first.
  struct Attachment {
    VulkanRenderTarget* target;
    bool is_depth;
    int version = 0;  // The version of the image in the framebuffer.
  };

  VulkanPass(VulkanBackend* backend, const RenderPassConfig& config,
             int pass_order);

  bool Init();

  VulkanBackend* const backend_;
  absl::InlinedVector<Attachment, 2> attachments_;
  VulkanRenderPassInfo render_pass_;
  vk::Framebuffer framebuffer_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_PASS_H_
