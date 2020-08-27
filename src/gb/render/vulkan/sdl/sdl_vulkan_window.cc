// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <algorithm>

#include "SDL_vulkan.h"
#include "absl/memory/memory.h"
#include "gb/message/message_system.h"
#include "gb/render/vulkan/sdl/sdl_vulkan_window.h"
#include "gb/render/vulkan/vulkan_types.h"
#include "glog/logging.h"

namespace gb {

std::unique_ptr<SdlVulkanWindow> SdlVulkanWindow::Create(Contract contract) {
  gb::ValidatedContext context = std::move(contract);
  if (!context.IsValid()) {
    return nullptr;
  }

  std::unique_ptr<gb::MessageEndpoint> endpoint;
  auto message_system = context.GetPtr<gb::MessageSystem>();
  auto sdl_endpoint_id =
      context.GetValue<gb::MessageEndpointId>(kKeySdlEndpointId);
  if (message_system != nullptr &&
      sdl_endpoint_id != gb::kNoMessageEndpointId) {
    endpoint = message_system->CreateEndpoint("SdlVulkanWindow");
    endpoint->Subscribe(sdl_endpoint_id);
  }

  return absl::WrapUnique(
      new SdlVulkanWindow(context.GetPtr<SDL_Window>(), std::move(endpoint)));
}

SdlVulkanWindow::SdlVulkanWindow(SDL_Window* window,
                                 std::unique_ptr<gb::MessageEndpoint> endpoint)
    : window_(window), endpoint_(std::move(endpoint)) {
  if (endpoint_ != nullptr) {
    endpoint_->SetHandler<SDL_Event>(
        [this](gb::MessageEndpointId, const SDL_Event& message) {
          OnEvent(message);
        });
  }
}

void SdlVulkanWindow::OnEvent(const SDL_Event& event) {
  if (event.type != SDL_WINDOWEVENT) {
    return;
  }
  if (event.window.event != SDL_WINDOWEVENT_RESIZED &&
      event.window.event != SDL_WINDOWEVENT_SIZE_CHANGED &&
      event.window.event != SDL_WINDOWEVENT_MINIMIZED &&
      event.window.event != SDL_WINDOWEVENT_MAXIMIZED &&
      event.window.event != SDL_WINDOWEVENT_RESTORED) {
    return;
  }
  absl::MutexLock lock(&mutex_);
  if (size_changed_callback_) {
    size_changed_callback_();
  }
}

void SdlVulkanWindow::SetSizeChangedCallback(
    gb::Callback<void()> size_changed_callback) {
  absl::MutexLock lock(&mutex_);
  size_changed_callback_ = std::move(size_changed_callback);
}

bool SdlVulkanWindow::GetExtensions(vk::Instance instance,
                                    std::vector<const char*>* extensions) {
  unsigned int extension_count;
  if (!SDL_Vulkan_GetInstanceExtensions(window_, &extension_count, nullptr)) {
    extensions->clear();
    return false;
  }
  extensions->resize(extension_count);
  if (!SDL_Vulkan_GetInstanceExtensions(window_, &extension_count,
                                        extensions->data())) {
    extensions->clear();
    return false;
  }
  return true;
}

vk::SurfaceKHR SdlVulkanWindow::CreateSurface(vk::Instance instance) {
  VkSurfaceKHR window_surface;
  if (!SDL_Vulkan_CreateSurface(window_, static_cast<VkInstance>(instance),
                                &window_surface)) {
    return {};
  }
  return window_surface;
}

vk::Extent2D SdlVulkanWindow::GetSize() {
  int width;
  int height;
  SDL_Vulkan_GetDrawableSize(window_, &width, &height);
  return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

}  // namespace gb
