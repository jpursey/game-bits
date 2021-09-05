// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_backend.h"

#include <vector>

#include "absl/memory/memory.h"
#include "gb/base/context_builder.h"
#include "gb/render/pixel_colors.h"
#include "gb/render/vulkan/vulkan_binding_data.h"
#include "gb/render/vulkan/vulkan_binding_data_factory.h"
#include "gb/render/vulkan/vulkan_descriptor_pool.h"
#include "gb/render/vulkan/vulkan_format.h"
#include "gb/render/vulkan/vulkan_render_buffer.h"
#include "gb/render/vulkan/vulkan_render_pipeline.h"
#include "gb/render/vulkan/vulkan_scene.h"
#include "gb/render/vulkan/vulkan_scene_type.h"
#include "gb/render/vulkan/vulkan_shader_code.h"
#include "gb/render/vulkan/vulkan_texture.h"
#include "gb/render/vulkan/vulkan_texture_array.h"
#include "gb/render/vulkan/vulkan_window.h"
#include "glog/logging.h"

//------------------------------------------------------------------------------
// Define Vulkan extension functions that will auto-load iff the extension is
// available, otherwise will fail with VK_ERROR_EXTENSION_NOT_PRESENT.

VkResult vkCreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void vkDestroyDebugUtilsMessengerEXT(VkInstance instance,
                                     VkDebugUtilsMessengerEXT debugMessenger,
                                     const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

namespace gb {

namespace {

static const vk::ClearColorValue kColorClearValue(std::array<float, 4>{
    0.0f, 0.0f, 0.0f, 1.0f});
static const vk::ClearDepthStencilValue kDepthClearValue(1.0f, 0);

}  // namespace

//------------------------------------------------------------------------------
// Helper functions and types.

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT c_message_severity,
    VkDebugUtilsMessageTypeFlagsEXT c_message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* c_callback_data,
    void* user_data) {
  const vk::DebugUtilsMessengerCallbackDataEXT callback_data(*c_callback_data);
  static_cast<VulkanBackend*>(user_data)->OnDebugMessage(
      static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(c_message_severity),
      static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(c_message_type),
      callback_data);
  return VK_FALSE;
}

//------------------------------------------------------------------------------
// VulkanBackend

std::unique_ptr<VulkanBackend> VulkanBackend::Create(Contract contract) {
  gb::ValidatedContext context(std::move(contract));
  if (!context.IsValid()) {
    return nullptr;
  }
  auto backend = absl::WrapUnique(new VulkanBackend(std::move(context)));
  if (!backend->Init()) {
    return nullptr;
  }
  return backend;
}

VulkanBackend::VulkanBackend(gb::ValidatedContext context)
    : debug_(context.GetValue<bool>(kKeyEnableDebug)),
      window_(context.GetPtr<VulkanWindow>()),
      frame_counter_(0),
      clear_color_(kColorClearValue),
      garbage_collector_index_(0) {
  context_ = std::move(context);
}

VulkanBackend::~VulkanBackend() { CleanUp(); }

bool VulkanBackend::Init() {
  std::vector<const char*> layers;
  if (!InitInstance(&layers) || !InitWindow() || !InitDevice(layers) ||
      !InitRenderPass() || !InitSwapChain() || !InitFrames() ||
      !InitResources()) {
    return false;
  }
  context_.SetPtr<VulkanBackend>(this);
  return true;
}

bool VulkanBackend::InitInstance(std::vector<const char*>* layers) {
  std::vector<const char*> extensions;
  if (!window_->GetExtensions(instance_, &extensions)) {
    LOG(ERROR) << "Failed to get required instance extensions";
    return false;
  }
  if (debug_) {
    layers->push_back("VK_LAYER_KHRONOS_validation");
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  std::string app_name = context_.GetValue<std::string>(kKeyAppName);
  auto app_info = vk::ApplicationInfo()
                      .setPApplicationName(app_name.c_str())
                      .setApplicationVersion(1)
                      .setPEngineName("Game Bits")
                      .setEngineVersion(1)
                      .setApiVersion(VK_API_VERSION_1_1);
  vk::InstanceCreateInfo instance_create_info =
      vk::InstanceCreateInfo()
          .setFlags(vk::InstanceCreateFlags())
          .setPApplicationInfo(&app_info)
          .setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
          .setPpEnabledExtensionNames(extensions.data())
          .setEnabledLayerCount(static_cast<uint32_t>(layers->size()))
          .setPpEnabledLayerNames(layers->data());

  vk::Result result;
  std::tie(result, instance_) = vk::createInstance(instance_create_info);
  if (result != vk::Result::eSuccess) {
    LOG(ERROR) << "Could not create a Vulkan instance: "
               << vk::to_string(result);
    return false;
  }

  if (debug_) {
    std::tie(result, debug_messenger_) = instance_.createDebugUtilsMessengerEXT(
        vk::DebugUtilsMessengerCreateInfoEXT()
            .setMessageSeverity(
                context_.GetValue<vk::DebugUtilsMessageSeverityFlagsEXT>())
            .setMessageType(
                context_.GetValue<vk::DebugUtilsMessageTypeFlagsEXT>())
            .setPfnUserCallback(DebugMessageCallback)
            .setPUserData(this));
    if (result != vk::Result::eSuccess) {
      return false;
    }
  }
  return true;
}

bool VulkanBackend::InitWindow() {
  window_surface_ = window_->CreateSurface(instance_);
  if (!window_surface_) {
    return false;
  }

  window_->SetSizeChangedCallback(
      callback_scope_.New<void()>([this] { recreate_swap_ = true; }));
  return true;
}

void VulkanBackend::InitFormat(
    const std::vector<vk::SurfaceFormatKHR>& formats) {
  format_ = formats[0];
  for (const auto& format : formats) {
    if (format.format == vk::Format::eB8G8R8A8Srgb &&
        format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      format_ = format;
      break;
    }
  }
}

bool VulkanBackend::InitDevice(const std::vector<const char*>& layers) {
  auto [result, physical_devices] = instance_.enumeratePhysicalDevices();
  if (result != vk::Result::eSuccess || physical_devices.empty()) {
    LOG(ERROR) << "No devices found supporting Vulkan.";
    return false;
  }
  std::vector<const char*> device_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  for (const auto& physical_device : physical_devices) {
    // Validate required features are available.
    auto features = physical_device.getFeatures();
    if (!features.samplerAnisotropy) {
      continue;
    }

    // Validate all needed queues are available.
    auto queue_families = physical_device.getQueueFamilyProperties();
    Queues queues;
    for (int i = 0; i < static_cast<int>(queue_families.size()); ++i) {
      const auto& queue_family = queue_families[i];
      if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
        queues.graphics_index = i;
      }
      vk::Bool32 supports_present_ = false;
      result = physical_device.getSurfaceSupportKHR(i, window_surface_,
                                                    &supports_present_);
      if (result != vk::Result::eSuccess) {
        LOG(ERROR) << "Failed to query surface support for queue family";
        return false;
      }
      if (supports_present_) {
        queues.present_index = i;
      }
      if (queues.IsComplete()) {
        break;
      }
    }
    if (!queues.IsComplete()) {
      continue;
    }

    // Validate all needed extensions are available.
    auto [result, extension_properties] =
        physical_device.enumerateDeviceExtensionProperties();
    std::set<std::string_view> needed_extensions(device_extensions.begin(),
                                                 device_extensions.end());
    for (const auto& extension_property : extension_properties) {
      if (needed_extensions.find(extension_property.extensionName) !=
          needed_extensions.end()) {
        needed_extensions.erase(extension_property.extensionName);
      }
    }
    if (!needed_extensions.empty()) {
      continue;
    }

    // Validate the swap chain is compatible.
    auto formats = physical_device.getSurfaceFormatsKHR(window_surface_).value;
    auto present_modes =
        physical_device.getSurfacePresentModesKHR(window_surface_).value;
    if (formats.empty() || present_modes.empty()) {
      continue;
    }

    // We have a usable device now, but prefer a discrete GPU if it exists.
    auto properties = physical_device.getProperties();
    if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
      physical_device_ = physical_device;
      queues_ = queues;
      InitFormat(formats);
      break;
    }
    if (!physical_device_) {
      physical_device_ = physical_device;
      queues_ = queues;
      InitFormat(formats);
    }
  }
  if (!physical_device_) {
    LOG(ERROR) << "No Vulkan device supports requirements.";
    return false;
  }

  // Initialize device specific properties.
  physical_device_properties_ = physical_device_.getProperties();
  auto depth_format =
      FindSupportedFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
                           vk::Format::eD24UnormS8Uint},
                          vk::ImageTiling::eOptimal,
                          vk::FormatFeatureFlagBits::eDepthStencilAttachment);
  if (!depth_format.has_value()) {
    LOG(ERROR) << "No available depth format";
    return false;
  }
  depth_format_ = depth_format.value();

  const vk::SampleCountFlags sample_counts =
      physical_device_properties_.limits.framebufferColorSampleCounts &
      physical_device_properties_.limits.framebufferDepthSampleCounts;
  if (sample_counts & vk::SampleCountFlagBits::e8) {
    msaa_count_ = vk::SampleCountFlagBits::e8;
  } else if (sample_counts & vk::SampleCountFlagBits::e4) {
    msaa_count_ = vk::SampleCountFlagBits::e4;
  } else if (sample_counts & vk::SampleCountFlagBits::e2) {
    msaa_count_ = vk::SampleCountFlagBits::e2;
  }

  // Create the logical device and retrieve the needed queues.
  std::set<uint32_t> queue_families = {queues_.graphics_index.value(),
                                       queues_.present_index.value()};
  std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
  queue_create_infos.reserve(queue_families.size());
  float queue_priorities = 1.0f;
  for (auto index : queue_families) {
    queue_create_infos.emplace_back()
        .setQueueFamilyIndex(index)
        .setQueueCount(1)
        .setPQueuePriorities(&queue_priorities);
  }
  auto device_features =
      vk::PhysicalDeviceFeatures().setSamplerAnisotropy(VK_TRUE);
  std::tie(result, device_) = physical_device_.createDevice(
      vk::DeviceCreateInfo()
          .setQueueCreateInfoCount(
              static_cast<uint32_t>(queue_create_infos.size()))
          .setPQueueCreateInfos(queue_create_infos.data())
          .setPEnabledFeatures(&device_features)
          .setEnabledLayerCount(static_cast<uint32_t>(layers.size()))
          .setPpEnabledLayerNames(layers.data())
          .setEnabledExtensionCount(
              static_cast<uint32_t>(device_extensions.size()))
          .setPpEnabledExtensionNames(device_extensions.data()));
  if (result != vk::Result::eSuccess) {
    LOG(ERROR) << "Failed to create logical device_.";
    return false;
  }
  queues_.graphics = device_.getQueue(queues_.graphics_index.value(), 0);
  queues_.present = device_.getQueue(queues_.present_index.value(), 0);

  VmaAllocatorCreateInfo allocator_create_info = {};
  // TODO: Use VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT if
  //       VK_EXT_memory_budget is available.
  allocator_create_info.flags = 0;
  allocator_create_info.physicalDevice = physical_device_;
  allocator_create_info.device = device_;
  allocator_create_info.preferredLargeHeapBlockSize = 0;  // Use default
  allocator_create_info.pAllocationCallbacks = NULL;  // Vulkan CPU allocation.
  allocator_create_info.pDeviceMemoryCallbacks = NULL;  // For tracking.
  allocator_create_info.frameInUseCount = 1;
  allocator_create_info.pHeapSizeLimit = NULL;
  allocator_create_info.pVulkanFunctions = NULL;
  allocator_create_info.pRecordSettings = NULL;  // Heap trace to file.
  allocator_create_info.instance = instance_;
  allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_1;
  if (vmaCreateAllocator(&allocator_create_info, &allocator_) != VK_SUCCESS) {
    LOG(ERROR) << "Failed to create allocator";
    return false;
  }
  return true;
}

bool VulkanBackend::InitRenderPass() {
  std::array<vk::AttachmentDescription, 3> attachments;

  auto color_attachement_ref =
      vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal);
  attachments[0]
      .setFormat(format_.format)
      .setSamples(msaa_count_)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

  auto depth_attachment_ref = vk::AttachmentReference(
      1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
  attachments[1]
      .setFormat(depth_format_)
      .setSamples(msaa_count_)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

  auto resolve_attachment_ref =
      vk::AttachmentReference(2, vk::ImageLayout::eColorAttachmentOptimal);
  attachments[2]
      .setFormat(format_.format)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

  auto subpass = vk::SubpassDescription()
                     .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                     .setColorAttachmentCount(1)
                     .setPColorAttachments(&color_attachement_ref)
                     .setPDepthStencilAttachment(&depth_attachment_ref)
                     .setPResolveAttachments(&resolve_attachment_ref);
  auto subpass_dependency =
      vk::SubpassDependency()
          .setSrcSubpass(VK_SUBPASS_EXTERNAL)
          .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
          .setSrcAccessMask({})
          .setDstSubpass(0)
          .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
          .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

  auto [create_result, render_pass] = device_.createRenderPass(
      vk::RenderPassCreateInfo()
          .setAttachmentCount(static_cast<uint32_t>(attachments.size()))
          .setPAttachments(attachments.data())
          .setSubpassCount(1)
          .setPSubpasses(&subpass)
          .setDependencyCount(1)
          .setPDependencies(&subpass_dependency));
  if (create_result != vk::Result::eSuccess) {
    LOG(ERROR) << "Failed to create render pass";
    return false;
  }
  render_pass_ = render_pass;
  return true;
}

bool VulkanBackend::InitSwapChain() {
  // Determine swap extent
  auto [capabilities_result, capabilities] =
      physical_device_.getSurfaceCapabilitiesKHR(window_surface_);
  if (capabilities_result != vk::Result::eSuccess) {
    LOG(ERROR) << "Unexpected error getting surface capabilities.";
    return false;
  }
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    swap_extent_ = capabilities.currentExtent;
  } else {
    auto size = window_->GetSize();
    swap_extent_.width =
        std::clamp(size.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    swap_extent_.height =
        std::clamp(size.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
  }
  {
    absl::MutexLock frame_dimensions_lock(&frame_dimensions_mutex_);
    frame_dimensions_ = {static_cast<int>(swap_extent_.width),
                         static_cast<int>(swap_extent_.height)};
  }
  if (swap_extent_.width == 0 || swap_extent_.height == 0) {
    return false;
  }

  // Determine the number of images.
  uint32_t image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount != 0) {
    image_count = std::min(image_count, capabilities.maxImageCount);
  }

  auto create_info =
      vk::SwapchainCreateInfoKHR()
          .setSurface(window_surface_)
          .setMinImageCount(image_count)
          .setImageFormat(format_.format)
          .setImageColorSpace(format_.colorSpace)
          .setImageExtent(swap_extent_)
          .setImageArrayLayers(1)
          .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
          .setImageSharingMode(vk::SharingMode::eExclusive)
          .setPreTransform(capabilities.currentTransform)
          .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
          .setPresentMode(vk::PresentModeKHR::eFifo)
          .setClipped(true);
  uint32_t queue_indices[2] = {queues_.graphics_index.value(),
                               queues_.present_index.value()};
  if (queue_indices[0] != queue_indices[1]) {
    create_info.setImageSharingMode(vk::SharingMode::eConcurrent)
        .setQueueFamilyIndexCount(2)
        .setPQueueFamilyIndices(queue_indices);
  }
  auto [create_result, swap_chain] = device_.createSwapchainKHR(create_info);
  if (create_result != vk::Result::eSuccess) {
    LOG(ERROR) << "Failed to create swapchain.";
    return false;
  }
  swap_chain_ = swap_chain;

  color_image_ = VulkanImage::Create(
      this, static_cast<int>(swap_extent_.width),
      static_cast<int>(swap_extent_.height), 1, format_.format,
      vk::ImageUsageFlagBits::eTransientAttachment |
          vk::ImageUsageFlagBits::eColorAttachment,
      VulkanImage::Options().SetSampleCount(msaa_count_));
  if (color_image_ == nullptr) {
    LOG(ERROR) << "Failed to create color image for swapchain";
    return false;
  }

  depth_image_ = VulkanImage::Create(
      this, static_cast<int>(swap_extent_.width),
      static_cast<int>(swap_extent_.height), 1, depth_format_,
      vk::ImageUsageFlagBits::eDepthStencilAttachment,
      VulkanImage::Options().SetSampleCount(msaa_count_));
  if (depth_image_ == nullptr) {
    LOG(ERROR) << "Failed to create depth image for swapchain";
    return false;
  }

  auto images = device_.getSwapchainImagesKHR(swap_chain_).value;
  frame_buffers_.reserve(images.size());
  for (const auto& image : images) {
    auto& buffer = frame_buffers_.emplace_back();
    buffer.image = image;
  }
  for (auto& buffer : frame_buffers_) {
    buffer.image_view = CreateImageView(buffer.image, format_.format);
    if (!buffer.image_view) {
      LOG(ERROR) << "Failed to create image view for swapchain";
      return false;
    }
    std::array<vk::ImageView, 3> attachments = {
        color_image_->GetView(), depth_image_->GetView(), buffer.image_view};
    auto [create_result, frame_buffer] = device_.createFramebuffer(
        vk::FramebufferCreateInfo()
            .setRenderPass(render_pass_)
            .setAttachmentCount(static_cast<uint32_t>(attachments.size()))
            .setPAttachments(attachments.data())
            .setWidth(swap_extent_.width)
            .setHeight(swap_extent_.height)
            .setLayers(1));
    if (create_result != vk::Result::eSuccess) {
      LOG(ERROR) << "Failed to create frame buffer";
      return false;
    }
    buffer.frame_buffer = frame_buffer;
  }
  return true;
}

bool VulkanBackend::InitFrames() {
  for (auto& frame : frames_) {
    if (!CreateSemaphore({}, &frame.image_available_semaphore) ||
        !CreateSemaphore({}, &frame.render_finished_semaphore) ||
        !CreateFence({vk::FenceCreateFlagBits::eSignaled},
                     &frame.render_finished_fence)) {
      LOG(ERROR) << "Failed to create frame synchronization objects";
      return false;
    }

    auto [create_pool_result, command_pool] = device_.createCommandPool(
        vk::CommandPoolCreateInfo()
            .setFlags(vk::CommandPoolCreateFlagBits::eTransient)
            .setQueueFamilyIndex(queues_.graphics_index.value()));
    if (create_pool_result != vk::Result::eSuccess) {
      LOG(ERROR) << "Failed to create frame command pool";
      return false;
    }
    frame.command_pool = command_pool;

    auto [result, command_buffers] = device_.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo()
            .setCommandPool(frame.command_pool)
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(1));
    if (result != vk::Result::eSuccess) {
      LOG(ERROR) << "Failed to create frame primary command buffer";
      return false;
    }
    frame.commands = command_buffers.front();
  }
  return true;
}

bool VulkanBackend::InitResources() { return true; }

void VulkanBackend::CleanUp() {
  LOG_IF(ERROR, !context_.Complete())
      << "Failed to complete VulkanBackend context.";
  if (device_) {
    auto result = device_.waitIdle();
    if (result != vk::Result::eSuccess) {
      LOG(ERROR) << "Wait idle failed on device";
    }
  }
  CleanUpSwap();
  if (render_pass_) {
    device_.destroyRenderPass(render_pass_);
  }
  {
    absl::MutexLock lock(&samplers_mutex_);
    for (const auto& it : samplers_) {
      device_.destroySampler(it.second);
    }
  }
  for (Frame& frame : frames_) {
    if (frame.image_available_semaphore) {
      device_.destroySemaphore(frame.image_available_semaphore);
    }
    if (frame.render_finished_semaphore) {
      device_.destroySemaphore(frame.render_finished_semaphore);
    }
    if (frame.render_finished_fence) {
      device_.destroyFence(frame.render_finished_fence);
    }
    if (frame.command_pool) {
      device_.destroyCommandPool(frame.command_pool);
    }
  }
  if (device_) {
    for (auto& garbage_collector : garbage_collectors_) {
      garbage_collector.Collect(device_, allocator_);
    }
    if (allocator_) {
      vmaDestroyAllocator(allocator_);
    }
    device_.destroy();
  }
  if (window_surface_) {
    instance_.destroySurfaceKHR(window_surface_);
    window_surface_ = nullptr;
  }
  if (debug_messenger_) {
    instance_.destroyDebugUtilsMessengerEXT(debug_messenger_);
    debug_messenger_ = nullptr;
  }
  if (instance_) {
    instance_.destroy();
  }
}

void VulkanBackend::CleanUpSwap() {
  for (auto& buffer : frame_buffers_) {
    if (buffer.frame_buffer) {
      device_.destroyFramebuffer(buffer.frame_buffer);
    }
    if (buffer.image_view) {
      device_.destroyImageView(buffer.image_view);
    }
  }
  frame_buffers_.clear();

  depth_image_.reset();
  color_image_.reset();

  if (swap_chain_) {
    device_.destroySwapchainKHR(swap_chain_);
    swap_chain_ = nullptr;
  }
}

bool VulkanBackend::RecreateSwap() {
  auto result = device_.waitIdle();
  if (result != vk::Result::eSuccess) {
    return false;
  }
  for (auto& garbage_collector : garbage_collectors_) {
    garbage_collector.Collect(device_, allocator_);
  }
  CleanUpSwap();
  return InitSwapChain();
}

void VulkanBackend::OnDebugMessage(
    vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
    vk::DebugUtilsMessageTypeFlagsEXT message_type,
    const vk::DebugUtilsMessengerCallbackDataEXT& callback_data) {
  LOG(INFO) << "Vulkan layer: " << callback_data.pMessage;
}

bool VulkanBackend::CreateSemaphore(const vk::SemaphoreCreateInfo& create_info,
                                    vk::Semaphore* semaphore) {
  return device_.createSemaphore(&create_info, nullptr, semaphore) ==
         vk::Result::eSuccess;
}

bool VulkanBackend::CreateFence(const vk::FenceCreateInfo& create_info,
                                vk::Fence* fence) {
  return device_.createFence(&create_info, nullptr, fence) ==
         vk::Result::eSuccess;
}

vk::ImageView VulkanBackend::CreateImageView(vk::Image image,
                                             vk::Format format) {
  auto [create_result, image_view] = device_.createImageView(
      vk::ImageViewCreateInfo()
          .setImage(image)
          .setViewType(vk::ImageViewType::e2D)
          .setFormat(format)
          .setSubresourceRange(
              vk::ImageSubresourceRange()
                  .setAspectMask(vk::ImageAspectFlagBits::eColor)
                  .setLevelCount(1)
                  .setLayerCount(1)));
  if (create_result != vk::Result::eSuccess) {
    return nullptr;
  }
  return image_view;
}

vk::Sampler VulkanBackend::GetSamplerWithValidation(SamplerOptions options,
                                                    int width, int height) {
  if (options.tile_size != 0 &&
      (width % options.tile_size != 0 || height % options.tile_size != 0)) {
    LOG(ERROR) << "Texture tile size " << options.tile_size
               << " is not evenly divisible into dimensions " << width << ","
               << height;
    return {};
  }

  if (options.mipmap) {
    if ((width & (width - 1)) != 0 || (height & (height - 1)) != 0) {
      LOG(ERROR) << "Texture dimensions " << width << "," << height
                 << " must be a power of two for mipmapping";
      return {};
    }
    if ((options.tile_size & (options.tile_size - 1)) != 0) {
      LOG(ERROR) << "Texture tile size " << options.tile_size
                 << " must be a power of two for mipmapping";
      return {};
    }
  }

  return GetSampler(options, width, height);
}

vk::Sampler VulkanBackend::GetSampler(SamplerOptions options, int width,
                                      int height) {
  if (options.mipmap) {
    if (options.tile_size == 0) {
      options.tile_size = std::min(width, height);
    }
  } else {
    // Tile size is unimportant if mipmapping is not enabled.
    options.tile_size = 0;
  }

  absl::MutexLock lock(&samplers_mutex_);
  auto it = samplers_.find(options);
  if (it != samplers_.end()) {
    return it->second;
  }

  const vk::Filter filter =
      (options.filter ? vk::Filter::eLinear : vk::Filter::eNearest);

  vk::SamplerAddressMode address_mode;
  switch (options.address_mode) {
    case SamplerAddressMode::kMirrorRepeat:
      address_mode = vk::SamplerAddressMode::eMirroredRepeat;
      break;
    case SamplerAddressMode::kClampEdge:
      address_mode = vk::SamplerAddressMode::eClampToEdge;
      break;
    case SamplerAddressMode::kClampBorder:
      address_mode = vk::SamplerAddressMode::eClampToBorder;
      break;
    case SamplerAddressMode::kRepeat:
    default:
      address_mode = vk::SamplerAddressMode::eRepeat;
  }

  vk::BorderColor border_color;
  if (options.border == Colors::kWhite) {
    border_color = vk::BorderColor::eIntOpaqueWhite;
  } else if (options.border == Colors::kBlack) {
    border_color = vk::BorderColor::eIntOpaqueBlack;
  } else if (options.border == Colors::kBlack.WithAlpha(0)) {
    border_color = vk::BorderColor::eIntTransparentBlack;
  } else {
    // TODO: Support other border colors VkSamplerCustomBorderColorCreateInfoEXT
    border_color =
        (options.border.a < 127 ? vk::BorderColor::eIntTransparentBlack
                                : vk::BorderColor::eIntOpaqueBlack);
  }

  const vk::SamplerMipmapMode mipmap_mode =
      (options.mipmap ? vk::SamplerMipmapMode::eLinear
                      : vk::SamplerMipmapMode::eNearest);
  float max_lod = 0.0f;
  if (options.mipmap) {
    int size = options.tile_size >> 1;
    while (size != 0) {
      size >>= 1;
      max_lod += 1.0f;
    }
  }

  auto [result, sampler] = device_.createSampler(
      vk::SamplerCreateInfo()
          .setMagFilter(filter)
          .setMinFilter(filter)
          .setAddressModeU(address_mode)
          .setAddressModeV(address_mode)
          .setAddressModeW(address_mode)
          .setAnisotropyEnable(options.filter ? VK_TRUE : VK_FALSE)
          .setMaxAnisotropy(options.filter ? 16.0f : 0.0f)
          .setBorderColor(border_color)
          .setUnnormalizedCoordinates(VK_FALSE)
          .setCompareEnable(VK_FALSE)
          .setCompareOp(vk::CompareOp::eAlways)
          .setMipmapMode(mipmap_mode)
          .setMipLodBias(0.0f)
          .setMinLod(0.0f)
          .setMaxLod(max_lod));
  if (result != vk::Result::eSuccess) {
    return {};
  }
  samplers_[options] = sampler;
  return sampler;
}

void VulkanBackend::SetClearColor(RenderInternal, Pixel color) {
  clear_color_.setFloat32(std::array<float, 4>({
      static_cast<float>(color.r) / 255.0f,
      static_cast<float>(color.g) / 255.0f,
      static_cast<float>(color.b) / 255.0f,
      static_cast<float>(color.a) / 255.0f,
  }));
}

FrameDimensions VulkanBackend::GetFrameDimensions(RenderInternal) const {
  absl::MutexLock lock(&frame_dimensions_mutex_);
  return frame_dimensions_;
}

std::optional<vk::Format> VulkanBackend::FindSupportedFormat(
    absl::Span<const vk::Format> formats, vk::ImageTiling tiling,
    vk::FormatFeatureFlags features) {
  for (vk::Format format : formats) {
    auto props = physical_device_.getFormatProperties(format);
    if (tiling == vk::ImageTiling::eLinear &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == vk::ImageTiling::eOptimal &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  return {};
}

void VulkanBackend::AddFrameCallback(RenderStage stage,
                                     FrameCallback callback) {
  RENDER_ASSERT(callback != nullptr);
  switch (stage) {
    case RenderStage::kBeginFrame:
      begin_frame_callbacks_.emplace_back(std::move(callback));
      break;
    case RenderStage::kBeginRender:
      begin_render_callbacks_.emplace_back(std::move(callback));
      break;
    case RenderStage::kEndRender:
      end_render_callbacks_.emplace_back(std::move(callback));
      break;
    case RenderStage::kPostRender:
      post_render_callbacks_.emplace_back(std::move(callback));
      break;
  }
}

Texture* VulkanBackend::CreateTexture(RenderInternal, gb::ResourceEntry entry,
                                      DataVolatility volatility, int width,
                                      int height,
                                      const SamplerOptions& options) {
  auto sampler = GetSamplerWithValidation(options, width, height);
  if (!sampler) {
    return nullptr;
  }
  return VulkanTexture::Create(std::move(entry), this, sampler, volatility,
                               width, height, options);
}

TextureArray* VulkanBackend::CreateTextureArray(
    RenderInternal, ResourceEntry entry, DataVolatility volatility, int count,
    int width, int height, const SamplerOptions& options) {
  auto sampler = GetSamplerWithValidation(options, width, height);
  if (!sampler) {
    return nullptr;
  }
  return VulkanTextureArray::Create(std::move(entry), this, sampler, volatility,
                                    count, width, height, options);
}

std::unique_ptr<ShaderCode> VulkanBackend::CreateShaderCode(RenderInternal,
                                                            const void* code,
                                                            int64_t code_size) {
  return VulkanShaderCode::Create({}, this, code, code_size);
}

std::unique_ptr<RenderSceneType> VulkanBackend::CreateSceneType(
    RenderInternal, absl::Span<const Binding> bindings) {
  auto scene_type = VulkanSceneType::Create({}, this, bindings);
  if (scene_type == nullptr) {
    return nullptr;
  }
  scene_types_.push_back(scene_type.get());
  return scene_type;
}

std::unique_ptr<RenderScene> VulkanBackend::CreateScene(
    RenderInternal, RenderSceneType* scene_type, int scene_order) {
  return VulkanScene::Create({}, static_cast<VulkanSceneType*>(scene_type),
                             scene_order);
}

std::unique_ptr<RenderPipeline> VulkanBackend::CreatePipeline(
    RenderInternal, RenderSceneType* scene_type, const VertexType* vertex_type,
    absl::Span<const Binding> bindings, ShaderCode* vertex_shader,
    ShaderCode* fragment_shader, const MaterialConfig& config) {
  return VulkanRenderPipeline::Create(
      {}, this, static_cast<VulkanSceneType*>(scene_type), vertex_type,
      bindings, static_cast<VulkanShaderCode*>(vertex_shader),
      static_cast<VulkanShaderCode*>(fragment_shader), config, render_pass_);
}

std::unique_ptr<RenderBuffer> VulkanBackend::CreateVertexBuffer(
    RenderInternal, DataVolatility volatility, int vertex_size,
    int vertex_capacity) {
  return VulkanRenderBuffer::Create({}, this, VulkanBufferType::kVertex,
                                    volatility, vertex_size, vertex_capacity);
}

std::unique_ptr<RenderBuffer> VulkanBackend::CreateIndexBuffer(
    RenderInternal, DataVolatility volatility, int index_capacity) {
  return VulkanRenderBuffer::Create({}, this, VulkanBufferType::kIndex,
                                    volatility, sizeof(uint16_t),
                                    index_capacity);
}

bool VulkanBackend::BeginFrame(RenderInternal) {
  auto& frame = frames_[frame_index_];
  render_state_.frame = GetFrame();

  // This wait is to ensure the command buffer for this frame is no longer
  // executing.
  auto result = device_.waitForFences({frame.render_finished_fence}, VK_TRUE,
                                      std::numeric_limits<uint64_t>::max());
  if (result != vk::Result::eSuccess) {
    return false;
  }

  // Any data from this frame is now unused, so collect the next set of garbage.
  // Note: memory_order_relaxed is used for the load, as this is the *only*
  // place that the garbage_collector_index_ is updated, and the subsequent
  // store will ensure proper ordering.
  const int next_gc_index =
      (garbage_collector_index_.load(std::memory_order_relaxed) + 1) %
      (kMaxFramesInFlight + 1);
  garbage_collectors_[next_gc_index].Collect(device_, allocator_);
  garbage_collector_index_.store(next_gc_index, std::memory_order_release);

  if (recreate_swap_) {
    auto size = window_->GetSize();
    if (size.width == 0 || size.height == 0) {
      // Cannot create a swapchain for a zero-sized window.
      return false;
    }
    if (!RecreateSwap()) {
      return false;
    }
    recreate_swap_ = false;
  }

  // If the size is zero, then there is no point continuing.
  if (swap_extent_.width == 0 || swap_extent_.height == 0) {
    return false;
  }

  auto [acquire_result, image_index] = device_.acquireNextImageKHR(
      swap_chain_, std::numeric_limits<uint64_t>::max(),
      frame.image_available_semaphore, {});
  if (acquire_result == vk::Result::eErrorOutOfDateKHR ||
      acquire_result == vk::Result::eSuboptimalKHR) {
    recreate_swap_ = true;
    return false;
  }
  if (acquire_result != vk::Result::eSuccess) {
    LOG_EVERY_N(ERROR, 100) << "Failed to acquire image from swapchain";
    return false;
  }
  frame_buffer_index_ = image_index;

  // Recreate the primary command buffer.
  device_.resetCommandPool(frame.command_pool, {});
  if (frame.commands.begin(vk::CommandBufferBeginInfo()) !=
      vk::Result::eSuccess) {
    LOG_EVERY_N(ERROR, 100) << "Failed to begin command buffer";
    return false;
  }

  CallFrameCallbacks(&begin_frame_callbacks_);

  return true;
}

void VulkanBackend::Draw(RenderInternal, RenderScene* scene,
                         RenderPipeline* pipeline, BindingData* material_data,
                         BindingData* instance_data, RenderBuffer* vertices,
                         RenderBuffer* indices) {
  render_state_.binding_data.insert(
      static_cast<VulkanBindingData*>(scene->GetSceneBindingData()));
  render_state_.binding_data.insert(
      static_cast<VulkanBindingData*>(material_data));
  render_state_.binding_data.insert(
      static_cast<VulkanBindingData*>(instance_data));
  render_state_.buffers.insert(static_cast<VulkanRenderBuffer*>(vertices));
  render_state_.buffers.insert(static_cast<VulkanRenderBuffer*>(indices));
  render_state_
      .draw[scene->GetOrder()][static_cast<VulkanScene*>(scene)]
           [static_cast<VulkanRenderPipeline*>(pipeline)]
      .mesh[static_cast<VulkanBindingData*>(material_data)]
           [static_cast<VulkanRenderBuffer*>(vertices)]
           [static_cast<VulkanRenderBuffer*>(indices)]
           [static_cast<VulkanBindingData*>(instance_data)->GetBufferGroup()]
      .push_back(static_cast<VulkanBindingData*>(instance_data));
}

void VulkanBackend::Draw(RenderInternal, RenderScene* scene,
                         absl::Span<const DrawCommand> commands) {
  render_state_.binding_data.insert(
      static_cast<VulkanBindingData*>(scene->GetSceneBindingData()));
  RenderPipeline* pipeline = nullptr;
  for (const auto& command : commands) {
    if (command.type == DrawCommand::Type::kPipeline) {
      if (pipeline == nullptr) {
        pipeline = command.pipeline;
      }
    } else if (command.type == DrawCommand::Type::kMaterialData ||
               command.type == DrawCommand::Type::kInstanceData) {
      render_state_.binding_data.insert(
          static_cast<VulkanBindingData*>(command.binding_data));
    } else if (command.type == DrawCommand::Type::kVertices ||
               command.type == DrawCommand::Type::kIndices) {
      render_state_.buffers.insert(
          static_cast<VulkanRenderBuffer*>(command.buffer));
    }
  }
  if (pipeline == nullptr) {
    return;
  }
  auto& draw =
      render_state_.draw[scene->GetOrder()][static_cast<VulkanScene*>(scene)]
                        [static_cast<VulkanRenderPipeline*>(pipeline)];
  draw.commands.insert(draw.commands.end(), commands.begin(), commands.end());
  draw.commands.emplace_back(DrawCommand::Type::kReset);
}

void VulkanBackend::EndFrame(RenderInternal) {
  EndFrameProcessUpdates();
  EndFrameRenderPass();
  EndFramePresent();
}

void VulkanBackend::CallFrameCallbacks(std::vector<FrameCallback>* callbacks) {
  auto& frame = frames_[frame_index_];
  auto callback_end = std::remove_if(
      callbacks->begin(), callbacks->end(),
      [commands = frame.commands](const FrameCallback& callback) {
        return !callback(commands);
      });
  if (callback_end != callbacks->end()) {
    callbacks->resize(callbacks->end() - callback_end);
  }
}

void VulkanBackend::EndFrameProcessUpdates() {
  // Binding data may add buffers and textures to the render state, so we need
  // to process them first.
  for (VulkanBindingData* binding_data : render_state_.binding_data) {
    binding_data->OnRender(&render_state_);
  }
  for (VulkanRenderBuffer* buffer : render_state_.buffers) {
    buffer->OnRender(&render_state_);
  }
  for (VulkanTexture* texture : render_state_.textures) {
    texture->OnRender(&render_state_);
  }
  for (VulkanTextureArray* texture_array : render_state_.texture_arrays) {
    texture_array->OnRender(&render_state_);
  }

  // Update all changed resources.
  if (!render_state_.buffer_updates.empty()) {
    EndFrameUpdateBuffers();
  }
  if (!render_state_.image_updates.empty()) {
    EndFrameUpdateImages();
  }
  EndFrameUpdateDescriptorSets();
}

void VulkanBackend::EndFrameUpdateBuffers() {
  auto& frame = frames_[frame_index_];

  std::vector<vk::BufferMemoryBarrier> barriers;
  barriers.reserve(render_state_.buffer_updates.size());

  for (const auto& update : render_state_.buffer_updates) {
    frame.commands.copyBuffer(update.src_buffer, update.dst_buffer,
                              {vk::BufferCopy(0, 0, update.copy_size)});
    auto& barrier = barriers.emplace_back();
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = update.dst_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = update.dst_buffer;
    barrier.offset = 0;
    barrier.size = update.copy_size;
  }

  // Create single pipeline barrier for any modified buffers.
  frame.commands.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                 vk::PipelineStageFlagBits::eVertexInput |
                                     vk::PipelineStageFlagBits::eVertexShader |
                                     vk::PipelineStageFlagBits::eFragmentShader,
                                 {}, {}, barriers, {});
}

void VulkanBackend::EndFrameUpdateImages() {
  auto& frame = frames_[frame_index_];

  std::vector<vk::ImageMemoryBarrier> barriers;
  barriers.reserve(render_state_.image_barriers.size());

  // Prep images for receiving new image data.
  for (size_t i = 0; i < render_state_.image_barriers.size(); ++i) {
    const auto& barrier = render_state_.image_barriers[i];
    barriers.emplace_back()
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setImage(barrier.image)
        .setSubresourceRange(vk::ImageSubresourceRange()
                                 .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                 .setBaseMipLevel(0)
                                 .setLevelCount(barrier.mip_level_count)
                                 .setBaseArrayLayer(barrier.layer)
                                 .setLayerCount(1))
        .setSrcAccessMask({})
        .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
  }
  frame.commands.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                 vk::PipelineStageFlagBits::eTransfer, {}, {},
                                 {}, barriers);

  // Copy the new data to the images.
  for (size_t i = 0; i < render_state_.image_updates.size(); ++i) {
    const auto& update = render_state_.image_updates[i];
    const vk::DeviceSize buffer_offset =
        update.src_offset +
        (update.region_y * update.image_width + update.region_x) *
            sizeof(Pixel);
    const uint32_t buffer_row_length =
        static_cast<uint32_t>(update.image_width);
    frame.commands.copyBufferToImage(
        update.src_buffer, update.dst_image,
        vk::ImageLayout::eTransferDstOptimal,
        {vk::BufferImageCopy()
             .setBufferOffset(buffer_offset)
             .setBufferRowLength(buffer_row_length)
             .setBufferImageHeight(0)
             .setImageSubresource(
                 vk::ImageSubresourceLayers()
                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                     .setMipLevel(update.mip_level)
                     .setBaseArrayLayer(update.image_layer)
                     .setLayerCount(1))
             .setImageOffset({update.region_x, update.region_y, 0})
             .setImageExtent({update.region_width, update.region_height, 1})});
  }

  // Prep the images for rendering.
  for (auto& barrier : barriers) {
    barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
        .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
        .setDstAccessMask(vk::AccessFlagBits::eShaderRead);
  }
  frame.commands.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                 vk::PipelineStageFlagBits::eFragmentShader, {},
                                 {}, {}, barriers);
}

void VulkanBackend::EndFrameUpdateDescriptorSets() {
  const size_t update_count = render_state_.set_image_updates.size() +
                              render_state_.set_buffer_updates.size();
  if (update_count == 0) {
    return;
  }

  std::vector<vk::WriteDescriptorSet> set_updates;
  set_updates.reserve(update_count);
  for (const auto& update : render_state_.set_image_updates) {
    set_updates.emplace_back()
        .setDstSet(update.descriptor_set)
        .setDstBinding(update.binding)
        .setDstArrayElement(0)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setPImageInfo(&update.info);
  }
  for (const auto& update : render_state_.set_buffer_updates) {
    set_updates.emplace_back()
        .setDstSet(update.descriptor_set)
        .setDstBinding(update.binding)
        .setDstArrayElement(0)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
        .setPBufferInfo(&update.info);
  }
  device_.updateDescriptorSets(static_cast<uint32_t>(set_updates.size()),
                               set_updates.data(), 0, nullptr);
}

void VulkanBackend::EndFrameRenderPass() {
  auto& frame = frames_[frame_index_];
  auto& frame_buffer = frame_buffers_[frame_buffer_index_];

  const std::array<vk::ClearValue, 2> kClearValues = {clear_color_,
                                                      kDepthClearValue};
  frame.commands.beginRenderPass(
      vk::RenderPassBeginInfo()
          .setRenderPass(render_pass_)
          .setFramebuffer(frame_buffer.frame_buffer)
          .setRenderArea({{0, 0}, swap_extent_})
          .setClearValueCount(static_cast<uint32_t>(kClearValues.size()))
          .setPClearValues(kClearValues.data()),
      vk::SubpassContents::eInline);

  auto scissor = vk::Rect2D({0, 0}, swap_extent_);
  auto viewport =
      vk::Viewport(0.0f, 0.0f, static_cast<float>(swap_extent_.width),
                   static_cast<float>(swap_extent_.height), 0.0f, 1.0f);
  frame.commands.setViewport(0, {viewport});
  frame.commands.setScissor(0, {scissor});

  CallFrameCallbacks(&begin_render_callbacks_);

  for (const auto& scene_group_it : render_state_.draw) {
    for (const auto& scene_it : scene_group_it.second) {
      VulkanScene* scene = scene_it.first;
      VulkanBindingData* scene_data =
          static_cast<VulkanBindingData*>(scene->GetSceneBindingData());
      auto scene_descriptor_set = scene_data->GetDescriptorSet(frame_index_);
      if (scene_descriptor_set) {
        frame.commands.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            scene_it.second.begin()->first->GetLayout(), 0,
            {scene_descriptor_set}, scene_data->GetBufferOffsets());
      }
      for (const auto& pipeline_it : scene_it.second) {
        VulkanRenderPipeline* pipeline = pipeline_it.first;
        frame.commands.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                    pipeline->Get());
        auto& pipeline_info = pipeline_it.second;
        for (const auto& material_it : pipeline_info.mesh) {
          VulkanBindingData* material_data = material_it.first;
          auto material_descriptor_set =
              material_data->GetDescriptorSet(frame_index_);
          if (material_descriptor_set) {
            frame.commands.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 1,
                {material_descriptor_set}, material_data->GetBufferOffsets());
          }
          for (const auto& vertex_it : material_it.second) {
            VulkanRenderBuffer* vertex_buffer = vertex_it.first;
            frame.commands.bindVertexBuffers(
                0, {vertex_buffer->GetBuffer(frame_index_)}, {0});
            for (const auto& index_it : vertex_it.second) {
              VulkanRenderBuffer* index_buffer = index_it.first;
              frame.commands.bindIndexBuffer(
                  index_buffer->GetBuffer(frame_index_), 0,
                  vk::IndexType::eUint16);
              for (const auto& group_it : index_it.second) {
                for (VulkanBindingData* instance_data : group_it.second) {
                  auto instance_descriptor_set =
                      instance_data->GetDescriptorSet(frame_index_);
                  if (instance_descriptor_set) {
                    frame.commands.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(),
                        2, {instance_descriptor_set},
                        instance_data->GetBufferOffsets());
                  }
                  frame.commands.drawIndexed(
                      static_cast<uint32_t>(index_buffer->GetSize()), 1, 0, 0,
                      0);
                }  // InstanceDraw
              }    // InstanceGroupDraw
            }      // IndexDraw
          }        // VertexDraw
        }          // MaterialDraw
        VulkanRenderPipeline* last_pipeline = pipeline;
        VulkanRenderPipeline* next_pipeline = pipeline;
        VulkanRenderBuffer* last_vertex_buffer = nullptr;
        VulkanRenderBuffer* next_vertex_buffer = nullptr;
        VulkanRenderBuffer* last_index_buffer = nullptr;
        VulkanRenderBuffer* next_index_buffer = nullptr;
        VulkanBindingData* last_material_data = nullptr;
        VulkanBindingData* next_material_data = nullptr;
        VulkanBindingData* last_instance_data = nullptr;
        VulkanBindingData* next_instance_data = nullptr;
        for (const auto& command : pipeline_info.commands) {
          switch (command.type) {
            case DrawCommand::Type::kPipeline: {
              pipeline = static_cast<VulkanRenderPipeline*>(command.pipeline);
              if (pipeline != last_pipeline) {
                next_pipeline = pipeline;
              }
              break;
            }
            case DrawCommand::Type::kVertices: {
              auto* buffer = static_cast<VulkanRenderBuffer*>(command.buffer);
              if (buffer != last_vertex_buffer) {
                next_vertex_buffer = buffer;
              }
              break;
            }
            case DrawCommand::Type::kIndices: {
              auto* buffer = static_cast<VulkanRenderBuffer*>(command.buffer);
              if (buffer != last_index_buffer) {
                next_index_buffer = buffer;
              }
              break;
            }
            case DrawCommand::Type::kMaterialData: {
              auto* binding_data =
                  static_cast<VulkanBindingData*>(command.binding_data);
              auto descriptor_set =
                  binding_data->GetDescriptorSet(frame_index_);
              if (binding_data != last_material_data && descriptor_set) {
                next_material_data = binding_data;
              }
              break;
            }
            case DrawCommand::Type::kInstanceData: {
              auto* binding_data =
                  static_cast<VulkanBindingData*>(command.binding_data);
              auto descriptor_set =
                  binding_data->GetDescriptorSet(frame_index_);
              if (binding_data != last_instance_data && descriptor_set) {
                next_instance_data = binding_data;
              }
              break;
            }
            case DrawCommand::Type::kScissor: {
              scissor.offset = vk::Offset2D{command.rect.x, command.rect.y};
              scissor.extent =
                  vk::Extent2D{command.rect.width, command.rect.height};
              frame.commands.setScissor(0, {scissor});
              break;
            }
            case DrawCommand::Type::kDraw: {
              if (next_pipeline != last_pipeline) {
                frame.commands.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                            next_pipeline->Get());
                last_pipeline = next_pipeline;
              }
              if (next_material_data != last_material_data) {
                frame.commands.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 1,
                    {next_material_data->GetDescriptorSet(frame_index_)},
                    next_material_data->GetBufferOffsets());
                last_material_data = next_material_data;
              }
              if (next_instance_data != last_instance_data) {
                frame.commands.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 2,
                    {next_instance_data->GetDescriptorSet(frame_index_)},
                    next_instance_data->GetBufferOffsets());
                last_instance_data = next_instance_data;
              }
              if (next_vertex_buffer != last_vertex_buffer) {
                frame.commands.bindVertexBuffers(
                    0, {next_vertex_buffer->GetBuffer(frame_index_)}, {0});
                last_vertex_buffer = next_vertex_buffer;
              }
              if (next_index_buffer != last_index_buffer) {
                frame.commands.bindIndexBuffer(
                    next_index_buffer->GetBuffer(frame_index_), 0,
                    vk::IndexType::eUint16);
                last_index_buffer = next_index_buffer;
              }
              frame.commands.drawIndexed(command.draw.index_count, 1,
                                         command.draw.index_offset,
                                         command.draw.vertex_offset, 0);
              break;
            }
            case DrawCommand::Type::kReset: {
              scissor = vk::Rect2D({0, 0}, swap_extent_);
              frame.commands.setScissor(0, {scissor});
              last_pipeline = nullptr;
              last_material_data = last_instance_data = nullptr;
              last_vertex_buffer = last_index_buffer = nullptr;
              break;
            }
          }
        }
      }  // PipelineDraw
    }    // SceneDraw
  }      // SceneGroupDraw

  CallFrameCallbacks(&end_render_callbacks_);

  frame.commands.endRenderPass();
}

void VulkanBackend::EndFramePresent() {
  auto& frame = frames_[frame_index_];
  auto& frame_buffer = frame_buffers_[frame_buffer_index_];

  // Call any registered callbacks
  CallFrameCallbacks(&post_render_callbacks_);

  if (frame.commands.end() != vk::Result::eSuccess) {
    // TODO: Handle this more gracefully...
    // There is not much we can do at this point, as the command buffer would
    // fail submission, and so we could never signal the render_finished_fence.
    LOG(FATAL) << "Failed to end primary command buffer";
  }

  // This wait is required, as vkAcquireNextImageKHR may never block for the
  // image to actually be available. As a result, it may not even be sent to the
  // presentation queue from a previous frame. This ensures that image has
  // entered the presentation queue, which means the image_available_semaphore
  // is valid to wait on for this execution.
  if (frame_buffer.render_finished_fence) {
    auto result =
        device_.waitForFences({frame_buffer.render_finished_fence}, VK_TRUE,
                              std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess) {
      // TODO: Handle this more gracefully...
      // If this happens we will be unrecoverably broken as the fence is in an
      // unknown state.
      LOG(FATAL) << "Failed to wait for render finished fence";
    }
  }
  frame_buffer.render_finished_fence = frame.render_finished_fence;
  device_.resetFences({frame.render_finished_fence});

  vk::PipelineStageFlags wait_stages =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  auto submit_result = queues_.graphics.submit(
      {vk::SubmitInfo()
           .setWaitSemaphoreCount(1)
           .setPWaitSemaphores(&frame.image_available_semaphore)
           .setPWaitDstStageMask(&wait_stages)
           .setCommandBufferCount(1)
           .setPCommandBuffers(&frame.commands)
           .setSignalSemaphoreCount(1)
           .setPSignalSemaphores(&frame.render_finished_semaphore)},
      frame.render_finished_fence);
  if (submit_result != vk::Result::eSuccess) {
    // TODO: Handle this more gracefully...
    // If this happens we will be unrecoverably broken as the fence is not
    // signaled, so BeginFrame will wait forever if it gets called, so we just
    // abort the program.
    LOG(FATAL) << "Failed to submit to graphics queue";
  }

  auto present_result = queues_.present.presentKHR(
      vk::PresentInfoKHR()
          .setWaitSemaphoreCount(1)
          .setPWaitSemaphores(&frame.render_finished_semaphore)
          .setSwapchainCount(1)
          .setPSwapchains(&swap_chain_)
          .setPImageIndices(&frame_buffer_index_));
  if (present_result == vk::Result::eErrorOutOfDateKHR ||
      present_result == vk::Result::eSuboptimalKHR) {
    recreate_swap_ = true;
    return;
  }
  if (present_result != vk::Result::eSuccess) {
    LOG_EVERY_N(ERROR, 100) << "Failed to present";
    return;
  }

  render_state_ = {};
  frame_index_ = (frame_index_ + 1) % kMaxFramesInFlight;

  // This the the only place that frame_counter_ is updated, and calls to this
  // method already must be externally synchronized, so there is no need to have
  // to use std::memory_order_acq_rel.
  frame_counter_.fetch_add(1, std::memory_order_release);
}

}  // namespace gb
