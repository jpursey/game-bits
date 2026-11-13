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

// Returns an attachment that clears the target's image as the pass begins, or
// keeps (loads) it.
vk::AttachmentDescription GetAttachment(vk::Format format, bool keep) {
  return vk::AttachmentDescription()
      .setFormat(format)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(keep ? vk::AttachmentLoadOp::eLoad
                      : vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(keep ? vk::ImageLayout::eShaderReadOnlyOptimal
                             : vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
}

}  // namespace

std::unique_ptr<VulkanPass> VulkanPass::Create(VulkanBackend* backend,
                                               const RenderPassConfig& config,
                                               int pass_order) {
  if (config.multisample) {
    LOG(ERROR) << "Multisampled passes are not supported by the Vulkan backend "
                  "yet";
    return nullptr;
  }
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
  absl::InlinedVector<vk::AttachmentDescription, 2> descriptions;
  vk::AttachmentReference color_ref;
  vk::AttachmentReference depth_ref;
  auto subpass = vk::SubpassDescription().setPipelineBindPoint(
      vk::PipelineBindPoint::eGraphics);
  VulkanPassLayout& layout = render_pass_.layout;
  for (const Attachment& attachment : attachments_) {
    const uint32_t index = static_cast<uint32_t>(descriptions.size());
    const vk::Format format =
        attachment.target->GetImageHandle().image->GetFormat();
    if (attachment.is_depth) {
      layout.depth_format = format;
      depth_ref.setAttachment(index).setLayout(
          vk::ImageLayout::eDepthStencilAttachmentOptimal);
      subpass.setPDepthStencilAttachment(&depth_ref);
      descriptions.push_back(GetAttachment(format, KeepsDepth()));
    } else {
      layout.color_format = format;
      color_ref.setAttachment(index).setLayout(
          vk::ImageLayout::eColorAttachmentOptimal);
      subpass.setColorAttachmentCount(1).setPColorAttachments(&color_ref);
      descriptions.push_back(GetAttachment(format, KeepsColor()));
    }
  }

  // Before the pass, wait for earlier reads and writes of the targets. After
  // it, make its writes visible to the shaders that sample them.
  const std::array<vk::SubpassDependency, 2> dependencies = {
      vk::SubpassDependency()
          .setSrcSubpass(VK_SUBPASS_EXTERNAL)
          .setSrcStageMask(kShaderStages | kAttachmentStages)
          .setSrcAccessMask(kAttachmentWrites)
          .setDstSubpass(0)
          .setDstStageMask(kAttachmentStages)
          .setDstAccessMask(kAttachmentReads | kAttachmentWrites),
      vk::SubpassDependency()
          .setSrcSubpass(0)
          .setSrcStageMask(kAttachmentStages)
          .setSrcAccessMask(kAttachmentWrites)
          .setDstSubpass(VK_SUBPASS_EXTERNAL)
          .setDstStageMask(kShaderStages)
          .setDstAccessMask(vk::AccessFlagBits::eShaderRead),
  };

  auto [result, render_pass] = backend_->GetDevice().createRenderPass(
      vk::RenderPassCreateInfo()
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

  absl::InlinedVector<vk::ImageView, 2> views;
  for (const Attachment& attachment : attachments_) {
    views.push_back(attachment.target->GetImageHandle().image->GetView());
  }
  const vk::Extent2D extent = GetExtent();
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

}  // namespace gb
