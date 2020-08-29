// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_BACKEND_H_
#define GB_RENDER_VULKAN_VULKAN_BACKEND_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gb/base/callback_scope.h"
#include "gb/base/validated_context.h"
#include "gb/render/render_backend.h"
#include "gb/render/vulkan/vulkan_allocator.h"
#include "gb/render/vulkan/vulkan_garbage_collector.h"
#include "gb/render/vulkan/vulkan_render_state.h"
#include "gb/render/vulkan/vulkan_types.h"
#include "glm/glm.hpp"

namespace gb {

class VulkanBackend final : public RenderBackend {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // Callbacks that can be registered to do additional work during BeginFrame
  // and EndFrame.
  //
  // This callback should return true for as long as it should continue to be
  // called on subsequent frames. If this returns false, it is implicitly
  // unregistered and will never be called again.
  //
  // The recommended approach when adding end frame callbacks is to use a
  // gb::CallbackScope to manage the lifecycle and automatically return false.
  using FrameCallback = gb::Callback<bool(vk::CommandBuffer commands)>;

  // Render stage within EndFrame.
  enum class RenderStage {
    kBeginFrame,   // At the beginning of a successful BeginFrame.
    kBeginRender,  // Right after render pass has begun.
    kEndRender,    // Right before render pass is ended.
    kPostRender,   // After render pass is completed, but before submission.
  };

  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: The application name which is passed to Vulkan.
  static inline constexpr char* kKeyAppName = "AppName";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintAppName, kInRequired,
                                     std::string, kKeyAppName);

  // REQUIRED: VulkanWindow interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintWindow, kInRequired, VulkanWindow);

  // OPTIONAL: Debug flag which controls whether validation and debug logging
  // are enabled.
  static inline constexpr char* kKeyEnableDebug = "EnableDebug";
#ifdef _DEBUG
  static inline constexpr bool kDefaultEnableDebug = true;
#else
  static inline constexpr bool kDefaultEnableDebug = false;
#endif
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintEnableDebug,
                                             kInOptional, bool, kKeyEnableDebug,
                                             kDefaultEnableDebug);

  // OPTIONAL: What Vulkan severity levels should be logged. This only has an
  // effect if "EnableDebug" is true in the context.
  static GB_CONTEXT_CONSTRAINT_DEFAULT(
      kConstraintDebugMessageSeverity, kInOptional,
      vk::DebugUtilsMessageSeverityFlagsEXT,
      vk::DebugUtilsMessageSeverityFlagsEXT{
          vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
          vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError});

  // OPTIONAL: What types of Vulkan messages should be logged. This only has an
  // effect if "EnableDebug" is true in the context.
  static GB_CONTEXT_CONSTRAINT_DEFAULT(
      kConstraintDebugMessageType, kInOptional,
      vk::DebugUtilsMessageTypeFlagsEXT,
      vk::DebugUtilsMessageTypeFlagsEXT{
          vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
          vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
          vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation});

  // SCOPED: This backend instance.
  static GB_CONTEXT_CONSTRAINT(kConstraintBackend, kScoped, VulkanBackend);

  using Contract =
      gb::ContextContract<kConstraintAppName, kConstraintWindow,
                          kConstraintEnableDebug,
                          kConstraintDebugMessageSeverity,
                          kConstraintDebugMessageType, kConstraintBackend>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  static std::unique_ptr<VulkanBackend> Create(Contract contract);
  ~VulkanBackend() override;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Vulkan handles
  vk::Instance GetInstance() const { return instance_; }
  vk::PhysicalDevice GetPhysicalDevice() const { return physical_device_; }
  const vk::PhysicalDeviceProperties& GetPhysicalDeviceProperties() {
    return physical_device_properties_;
  }
  vk::Device GetDevice() const { return device_; }
  uint32_t GetGraphicsQueueFamilyIndex() const {
    return queues_.graphics_index.value();
  }
  vk::Queue GetGraphicsQueue() const { return queues_.graphics; }
  int GetSwapImageCount() const {
    return static_cast<int>(frame_buffers_.size());
  }
  vk::RenderPass GetRenderPass() const { return render_pass_; }

  // Allocator
  VmaAllocator GetAllocator() { return allocator_; }

  // Garbage collector that defers destruction until frame is not in use.
  VulkanGarbageCollector* GetGarbageCollector() {
    return &garbage_collectors_[garbage_collector_index_];
  }

  // Current frame
  int GetFrame() const { return frame_counter_; }

  //----------------------------------------------------------------------------
  // Helper methods
  //----------------------------------------------------------------------------

  std::optional<vk::Format> VulkanBackend::FindSupportedFormat(
      absl::Span<const vk::Format> formats, vk::ImageTiling tiling,
      vk::FormatFeatureFlags features);

  //----------------------------------------------------------------------------
  // Backend hooks
  //----------------------------------------------------------------------------

  void AddFrameCallback(RenderStage stage, FrameCallback callback);

  //----------------------------------------------------------------------------
  // RenderBackend overrides
  //----------------------------------------------------------------------------

  FrameDimensions GetFrameDimensions(RenderInternal) const override;

  Texture* CreateTexture(RenderInternal, gb::ResourceEntry entry,
                         DataVolatility volatility, int width,
                         int height) override;
  std::unique_ptr<ShaderCode> CreateShaderCode(RenderInternal, const void* code,
                                               int64_t code_size) override;
  std::unique_ptr<RenderSceneType> CreateSceneType(
      RenderInternal, absl::Span<const Binding> bindings) override;
  std::unique_ptr<RenderScene> CreateScene(RenderInternal,
                                           RenderSceneType* scene_type,
                                           int scene_order) override;
  std::unique_ptr<RenderPipeline> CreatePipeline(
      RenderInternal, RenderSceneType* scene_type,
      const VertexType* vertex_type, absl::Span<const Binding> bindings,
      ShaderCode* vertex_shader, ShaderCode* fragment_shader,
      const MaterialConfig& config) override;
  std::unique_ptr<RenderBuffer> CreateVertexBuffer(
      RenderInternal, DataVolatility volatility, int vertex_size,
      int vertex_capacity) override;
  std::unique_ptr<RenderBuffer> CreateIndexBuffer(RenderInternal,
                                                  DataVolatility volatility,
                                                  int index_capacity) override;

  bool BeginFrame(RenderInternal) override;
  void Draw(RenderInternal, RenderScene* scene, RenderPipeline* pipeline,
            BindingData* material_data, BindingData* instance_data,
            RenderBuffer* vertices, RenderBuffer* indices) override;
  void EndFrame(RenderInternal) override;

 private:
  struct Queues {
    Queues() = default;

    std::optional<uint32_t> graphics_index;
    vk::Queue graphics;

    std::optional<uint32_t> present_index;
    vk::Queue present;

    bool IsComplete() const {
      return graphics_index.has_value() && present_index.has_value();
    }
  };

  struct FrameBuffer {
    vk::Image image;
    vk::ImageView image_view;
    vk::Framebuffer frame_buffer;
    vk::Fence render_finished_fence;
  };

  struct Frame {
    vk::Semaphore image_available_semaphore;
    vk::Semaphore render_finished_semaphore;
    vk::Fence render_finished_fence;
    vk::CommandPool command_pool;
    vk::CommandBuffer commands;
  };

  VulkanBackend(gb::ValidatedContext context);

  bool Init();
  bool InitInstance(std::vector<const char*>* layers);
  bool InitWindow();
  bool InitDevice(const std::vector<const char*>& layers);
  void InitFormat(const std::vector<vk::SurfaceFormatKHR>& formats);
  bool InitRenderPass();
  bool InitSwapChain();
  bool InitFrames();
  bool InitResources();
  void CleanUp();
  void CleanUpSwap();
  bool RecreateSwap();

  friend VkBool32 DebugMessageCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
      VkDebugUtilsMessageTypeFlagsEXT message_type,
      const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
      void* user_data);
  void OnDebugMessage(
      vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
      vk::DebugUtilsMessageTypeFlagsEXT message_type,
      const vk::DebugUtilsMessengerCallbackDataEXT& callback_data);

  bool CreateSemaphore(const vk::SemaphoreCreateInfo& create_info,
                       vk::Semaphore* semaphore);
  bool CreateFence(const vk::FenceCreateInfo& create_info, vk::Fence* fence);
  vk::ImageView CreateImageView(vk::Image image, vk::Format format);

  void CallFrameCallbacks(std::vector<FrameCallback>* callbacks);
  void EndFrameProcessUpdates();
  void EndFrameUpdateBuffers();
  void EndFrameUpdateImages();
  void EndFrameUpdateDescriptorSets();
  void EndFrameRenderPass();
  void EndFramePresent();

  // General properties
  gb::ValidatedContext context_;
  const bool debug_;

  // Instance
  vk::Instance instance_;
  vk::DebugUtilsMessengerEXT debug_messenger_;

  // Window
  VulkanWindow* window_;
  vk::SurfaceKHR window_surface_;
  vk::SurfaceFormatKHR format_;

  // Device
  vk::PhysicalDevice physical_device_;
  vk::PhysicalDeviceProperties physical_device_properties_;
  vk::FormatProperties physical_format_properties_;
  vk::Device device_;
  Queues queues_;
  vk::Format depth_format_;
  VmaAllocator allocator_ = VK_NULL_HANDLE;

  // Render pass
  vk::RenderPass render_pass_;

  // Swap chain
  vk::Extent2D swap_extent_;
  vk::SwapchainKHR swap_chain_;
  std::vector<FrameBuffer> frame_buffers_;
  std::unique_ptr<VulkanImage> depth_image_;
  bool recreate_swap_ = false;

  // Frame management
  FrameCounter frame_counter_ = 0;
  int frame_index_ = 0;
  uint32_t frame_buffer_index_ = 0;
  std::array<Frame, kMaxFramesInFlight> frames_;
  VulkanRenderState render_state_;

  // Global resources
  vk::Sampler sampler_;
  std::vector<VulkanSceneType*> scene_types_;

  // Garbage collection
  std::array<VulkanGarbageCollector, kMaxFramesInFlight + 1>
      garbage_collectors_;
  int garbage_collector_index_ = 0;

  // Callbacks
  std::vector<FrameCallback> begin_frame_callbacks_;
  std::vector<FrameCallback> begin_render_callbacks_;
  std::vector<FrameCallback> end_render_callbacks_;
  std::vector<FrameCallback> post_render_callbacks_;

  // Must be last
  gb::CallbackScope callback_scope_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_BACKEND_H_
