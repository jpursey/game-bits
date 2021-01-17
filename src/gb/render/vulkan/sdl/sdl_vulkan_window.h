// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_SDL_VULKAN_WINDOW_H_
#define GB_RENDER_VULKAN_SDL_VULKAN_WINDOW_H_

#include <memory>

#include "SDL.h"
#include "absl/synchronization/mutex.h"
#include "gb/base/validated_context.h"
#include "gb/message/message_endpoint.h"
#include "gb/message/message_types.h"
#include "gb/render/vulkan/vulkan_window.h"

namespace gb {

// Implements VulkanWindow in terms of an SDL_Window.
class SdlVulkanWindow final : public VulkanWindow {
 public:
  //----------------------------------------------------------------------------
  // SdlVulkanWindow constraints
  //----------------------------------------------------------------------------

  // REQUIRED: Window this is wrapping. This pointer must remain valid for the
  // lifetime of this class.
  static GB_CONTEXT_CONSTRAINT(kConstraintWindow, kInRequired, SDL_Window);

  // OPTIONAL: MessageSystem that has a channel defined by
  // kConstraintSdlEndpointId which sends SDL_Event messages.
  static GB_CONTEXT_CONSTRAINT(kConstraintMessageSystem, kInOptional,
                               gb::MessageSystem);

  // OPTIONAL: Message channel ID on the corresponding kConstraintMessageSystem
  // which sends SDL_Event messages.
  static inline constexpr const char* kKeySdlEndpointId = "SdlEndpointId";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintSdlEndpointId, kInOptional,
                                     gb::MessageEndpointId, kKeySdlEndpointId);

  using Contract =
      gb::ContextContract<kConstraintWindow, kConstraintMessageSystem,
                          kConstraintSdlEndpointId>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Constructs with valid contract.
  static std::unique_ptr<SdlVulkanWindow> Create(Contract contract);
  SdlVulkanWindow(const SdlVulkanWindow&) = delete;
  SdlVulkanWindow& operator=(const SdlVulkanWindow&) = delete;
  ~SdlVulkanWindow() override = default;

  //----------------------------------------------------------------------------
  // VulkanWindow interface
  //----------------------------------------------------------------------------

  void SetSizeChangedCallback(
      gb::Callback<void()> size_changed_callback) override;
  bool GetExtensions(vk::Instance instance,
                     std::vector<const char*>* extensions) override;
  vk::SurfaceKHR CreateSurface(vk::Instance instance) override;
  vk::Extent2D GetSize() override;

 private:
  SdlVulkanWindow(SDL_Window* window,
                  std::unique_ptr<gb::MessageEndpoint> endpoint);

  void OnEvent(const SDL_Event& event);

  SDL_Window* window_;
  std::unique_ptr<gb::MessageEndpoint> endpoint_;

  absl::Mutex mutex_;
  gb::Callback<void()> size_changed_callback_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_SDL_VULKAN_WINDOW_H_
