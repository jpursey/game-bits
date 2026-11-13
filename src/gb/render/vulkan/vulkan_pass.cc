// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_pass.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_format.h"

namespace gb {

namespace {

// The stages that read and write attachments.
constexpr vk::PipelineStageFlags kAttachmentStages =
    vk::PipelineStageFlagBits::eEarlyFragmentTests |
    vk::PipelineStageFlagBits::eLateFragmentTests |
    vk::PipelineStageFlagBits::eColorAttachmentOutput;
constexpr vk::AccessFlags kAttachmentWrites =
    vk::AccessFlagBits::eColorAttachmentWrite |
    vk::AccessFlagBits::eDepthStencilAttachmentWrite;
constexpr vk::AccessFlags kAttachmentReads =
    vk::AccessFlagBits::eColorAttachmentRead |
    vk::AccessFlagBits::eDepthStencilAttachmentRead;

// The stages that sample targets.
constexpr vk::PipelineStageFlags kShaderStages =
    vk::PipelineStageFlagBits::eVertexShader |
    vk::PipelineStageFlagBits::eFragmentShader;

// Returns an attachment for a target's image, which is ready to sample once
// the pass ends. It keeps (loads) what the image held, clears it, or does
// neither, for one that is resolved into.
vk::AttachmentDescription2 GetTargetAttachment(vk::Format format,
                                               vk::AttachmentLoadOp load_op) {
  return vk::AttachmentDescription2()
      .setFormat(format)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(load_op)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(load_op == vk::AttachmentLoadOp::eLoad
                            ? vk::ImageLayout::eShaderReadOnlyOptimal
                            : vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
}

// Returns an attachment for a multisampled image, which is cleared as the pass
// begins, and resolved as it ends.
//
// Color is thrown away once it is resolved. Depth is stored, though nothing
// reads it after its resolve: NVIDIA's driver leaves parts of the resolved
// depth undefined at some sizes when it isn't.
vk::AttachmentDescription2 GetSampleAttachment(
    vk::Format format, vk::SampleCountFlagBits sample_count, bool is_depth) {
  return vk::AttachmentDescription2()
      .setFormat(format)
      .setSamples(sample_count)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(is_depth ? vk::AttachmentStoreOp::eStore
                           : vk::AttachmentStoreOp::eDontCare)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(is_depth ? vk::ImageLayout::eDepthStencilAttachmentOptimal
                               : vk::ImageLayout::eColorAttachmentOptimal);
}

}  // namespace

std::unique_ptr<VulkanPass> VulkanPass::Create(VulkanBackend* backend,
                                               const RenderPassConfig& config,
                                               int pass_order) {
  auto pass = absl::WrapUnique(new VulkanPass(backend, config, pass_order));
  if (!pass->Init()) {
    return nullptr;
  }
  return pass;
}

VulkanPass::VulkanPass(VulkanBackend* backend, const RenderPassConfig& config,
                       int pass_order)
    : RenderPass(config, pass_order), backend_(backend) {
  if (GetColorTarget() != nullptr) {
    attachments_.push_back(
        {static_cast<VulkanRenderTarget*>(GetColorTarget()), false});
  }
  if (GetDepthTarget() != nullptr) {
    attachments_.push_back(
        {static_cast<VulkanRenderTarget*>(GetDepthTarget()), true});
  }
}

VulkanPass::~VulkanPass() {
  auto* gc = backend_->GetGarbageCollector();
  gc->Dispose(framebuffer_);
  gc->Dispose(render_pass_.render_pass);
}

bool VulkanPass::Init() {
  VulkanPassLayout& layout = render_pass_.layout;
  if (IsMultisampled()) {
    layout.sample_count = backend_->GetMsaaSampleCount();
  }
  const bool resolves = Resolves();

  // The pass draws into an attachment per target. In a multisampled pass,
  // these are the multisampled images, and an attachment per target follows
  // them, each resolved into: color by its subpass's resolve attachment, and
  // depth by its depth resolve.
  const int count = static_cast<int>(attachments_.size());
  absl::InlinedVector<vk::AttachmentDescription2, 4> descriptions(
      resolves ? count * 2 : count);
  vk::AttachmentReference2 color_ref;
  vk::AttachmentReference2 depth_ref;
  vk::AttachmentReference2 color_resolve_ref;
  vk::AttachmentReference2 depth_resolve_ref;
  auto depth_resolve =
      vk::SubpassDescriptionDepthStencilResolve()
          .setDepthResolveMode(vk::ResolveModeFlagBits::eSampleZero)
          .setStencilResolveMode(vk::ResolveModeFlagBits::eNone)
          .setPDepthStencilResolveAttachment(&depth_resolve_ref);
  auto subpass = vk::SubpassDescription2().setPipelineBindPoint(
      vk::PipelineBindPoint::eGraphics);
  for (int i = 0; i < count; ++i) {
    const Attachment& attachment = attachments_[i];
    const vk::Format format =
        attachment.target->GetImageHandle().image->GetFormat();
    const vk::ImageLayout draw_layout =
        attachment.is_depth ? vk::ImageLayout::eDepthStencilAttachmentOptimal
                            : vk::ImageLayout::eColorAttachmentOptimal;
    const auto index = static_cast<uint32_t>(i);
    const auto resolve_index = static_cast<uint32_t>(count + i);
    if (attachment.is_depth) {
      layout.depth_format = format;
      depth_ref.setAttachment(index).setLayout(draw_layout);
      depth_resolve_ref.setAttachment(resolve_index).setLayout(draw_layout);
      subpass.setPDepthStencilAttachment(&depth_ref);
      if (resolves) {
        subpass.setPNext(&depth_resolve);
      }
    } else {
      layout.color_format = format;
      color_ref.setAttachment(index).setLayout(draw_layout);
      color_resolve_ref.setAttachment(resolve_index).setLayout(draw_layout);
      subpass.setColorAttachmentCount(1).setPColorAttachments(&color_ref);
      if (resolves) {
        subpass.setPResolveAttachments(&color_resolve_ref);
      }
    }
    if (resolves) {
      descriptions[i] =
          GetSampleAttachment(format, layout.sample_count, attachment.is_depth);
      descriptions[count + i] =
          GetTargetAttachment(format, vk::AttachmentLoadOp::eDontCare);
    } else {
      const bool keep = attachment.is_depth ? KeepsDepth() : KeepsColor();
      descriptions[i] =
          GetTargetAttachment(format, keep ? vk::AttachmentLoadOp::eLoad
                                           : vk::AttachmentLoadOp::eClear);
    }
  }

  // Before the pass, wait for earlier reads and writes of the targets. After
  // it, make its writes visible to the shaders that sample them. Resolves are
  // color attachment writes, for depth too.
  const std::array<vk::SubpassDependency2, 2> dependencies = {
      vk::SubpassDependency2()
          .setSrcSubpass(VK_SUBPASS_EXTERNAL)
          .setSrcStageMask(kShaderStages | kAttachmentStages)
          .setSrcAccessMask(kAttachmentWrites)
          .setDstSubpass(0)
          .setDstStageMask(kAttachmentStages)
          .setDstAccessMask(kAttachmentReads | kAttachmentWrites),
      vk::SubpassDependency2()
          .setSrcSubpass(0)
          .setSrcStageMask(kAttachmentStages)
          .setSrcAccessMask(kAttachmentWrites)
          .setDstSubpass(VK_SUBPASS_EXTERNAL)
          .setDstStageMask(kShaderStages)
          .setDstAccessMask(vk::AccessFlagBits::eShaderRead),
  };

  auto [result, render_pass] = backend_->GetDevice().createRenderPass2(
      vk::RenderPassCreateInfo2()
          .setAttachmentCount(static_cast<uint32_t>(descriptions.size()))
          .setPAttachments(descriptions.data())
          .setSubpassCount(1)
          .setPSubpasses(&subpass)
          .setDependencyCount(static_cast<uint32_t>(dependencies.size()))
          .setPDependencies(dependencies.data()));
  if (result != vk::Result::eSuccess) {
    LOG(ERROR) << "Failed to create render pass";
    return false;
  }
  render_pass_.render_pass = render_pass;
  return true;
}

vk::Extent2D VulkanPass::GetExtent() const {
  // The targets are the same size, unless resizing one of them failed, in
  // which case the pass draws where they overlap.
  int width = std::numeric_limits<int>::max();
  int height = std::numeric_limits<int>::max();
  for (const Attachment& attachment : attachments_) {
    const VulkanImage* image = attachment.target->GetImageHandle().image;
    width = std::min(width, image->GetWidth());
    height = std::min(height, image->GetHeight());
  }
  return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

absl::InlinedVector<vk::ClearValue, 2> VulkanPass::GetClearValues() const {
  absl::InlinedVector<vk::ClearValue, 2> clear_values;
  for (const Attachment& attachment : attachments_) {
    if (attachment.is_depth) {
      clear_values.emplace_back(kDepthClearValue);
    } else {
      clear_values.emplace_back(GetClearColorValue(GetClearColor()));
    }
  }
  return clear_values;
}

void VulkanPass::OnRender(VulkanRenderState* state) {
  for (const Attachment& attachment : attachments_) {
    attachment.target->OnRender(state);
  }
}

vk::Framebuffer VulkanPass::GetFramebuffer() {
  bool stale = !framebuffer_;
  for (const Attachment& attachment : attachments_) {
    stale |= attachment.version != attachment.target->GetImageHandle().version;
  }
  if (!stale) {
    return framebuffer_;
  }

  // A framebuffer replaced is destroyed once no frame in flight uses it.
  backend_->GetGarbageCollector()->Dispose(framebuffer_);
  framebuffer_ = nullptr;

  const vk::Extent2D extent = GetExtent();
  const bool resolves = Resolves();
  if (resolves && !CreateSampleImages(extent)) {
    LOG_EVERY_N(ERROR, 100) << "Failed to create a multisampled pass's images";
    return nullptr;
  }

  // The views are in the render pass's attachment order (see Init).
  const int count = static_cast<int>(attachments_.size());
  absl::InlinedVector<vk::ImageView, 4> views(resolves ? count * 2 : count);
  for (int i = 0; i < count; ++i) {
    const Attachment& attachment = attachments_[i];
    const vk::ImageView target_view =
        attachment.target->GetImageHandle().image->GetView();
    if (resolves) {
      views[i] = attachment.samples->GetView();
      views[count + i] = target_view;
    } else {
      views[i] = target_view;
    }
  }
  auto [result, framebuffer] = backend_->GetDevice().createFramebuffer(
      vk::FramebufferCreateInfo()
          .setRenderPass(render_pass_.render_pass)
          .setAttachmentCount(static_cast<uint32_t>(views.size()))
          .setPAttachments(views.data())
          .setWidth(extent.width)
          .setHeight(extent.height)
          .setLayers(1));
  if (result != vk::Result::eSuccess) {
    LOG_EVERY_N(ERROR, 100) << "Failed to create a render pass's framebuffer";
    return nullptr;
  }
  framebuffer_ = framebuffer;
  for (Attachment& attachment : attachments_) {
    attachment.version = attachment.target->GetImageHandle().version;
  }
  return framebuffer_;
}

bool VulkanPass::CreateSampleImages(vk::Extent2D extent) {
  for (Attachment& attachment : attachments_) {
    // Color is never stored, so it is transient. Depth is stored (see
    // GetSampleAttachment).
    const vk::ImageUsageFlags usage =
        attachment.is_depth
            ? vk::ImageUsageFlags(
                  vk::ImageUsageFlagBits::eDepthStencilAttachment)
            : vk::ImageUsageFlagBits::eTransientAttachment |
                  vk::ImageUsageFlagBits::eColorAttachment;

    // An image replaced is destroyed once no frame in flight uses it.
    attachment.samples = VulkanImage::Create(
        backend_, static_cast<int>(extent.width),
        static_cast<int>(extent.height), 1,
        attachment.target->GetImageHandle().image->GetFormat(), usage,
        VulkanImage::Options().SetSampleCount(
            render_pass_.layout.sample_count));
    if (attachment.samples == nullptr) {
      return false;
    }
  }
  return true;
}

}  // namespace gb
