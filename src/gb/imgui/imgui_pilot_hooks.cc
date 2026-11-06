// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/imgui/imgui_pilot_hooks.h"

#include <cmath>

#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include "imgui_internal.h"

namespace gb {
namespace internal {

namespace {

// Returns the text ImGui shows for a label or window name, which ends before
// any "##".
std::string_view GetShownText(const char* text) {
  return std::string_view(text, ImGui::FindRenderedTextEnd(text) - text);
}

// Returns the window a click at the point goes to, or null if none. Like
// ImGui's own search for the hovered window, this is the front window that
// takes mouse input and contains the point, though it leaves out the border a
// window is resized from.
const ImGuiWindow* FindFrontWindow(const ImGuiContext* context,
                                   PilotPoint point) {
  const ImVec2 position(static_cast<float>(point.x),
                        static_cast<float>(point.y));
  for (int i = context->Windows.Size - 1; i >= 0; --i) {
    const ImGuiWindow* window = context->Windows[i];
    if (window->Active && !window->Hidden &&
        (window->Flags & ImGuiWindowFlags_NoMouseInputs) == 0 &&
        window->OuterRectClipped.Contains(position)) {
      return window;
    }
  }
  return nullptr;
}

void CallBeginFrame(ImGuiContext* context, ImGuiContextHook* hook) {
  static_cast<ImGuiPilotHooks*>(hook->UserData)->BeginFrame();
}

}  // namespace

ImGuiPilotHooks::ImGuiPilotHooks(Pilot* pilot, ImGuiContext* context)
    : pilot_(pilot), context_(context) {
  ImGuiContextHook hook;
  hook.UserData = this;
  hook.Type = ImGuiContextHookType_NewFramePre;
  hook.Callback = CallBeginFrame;
  new_frame_hook_ = ImGui::AddContextHook(context_, &hook);

  // The test engine hooks find this as the test engine.
  context_->TestEngine = this;
  context_->TestEngineHookItems = true;

  item_finder_ = pilot_->AddItemFinder(
      [this](std::string_view label) { return FindItems(label); });
}

ImGuiPilotHooks::~ImGuiPilotHooks() {
  context_->TestEngineHookItems = false;
  context_->TestEngine = nullptr;
  ImGui::RemoveContextHook(context_, new_frame_hook_);
}

void ImGuiPilotHooks::BeginFrame() {
  ImGuiIO& io = context_->IO;
  const PilotMouse mouse = pilot_->GetMouse();
  io.MousePos = ImVec2(static_cast<float>(mouse.position.x),
                       static_cast<float>(mouse.position.y));
  io.MouseDown[ImGuiMouseButton_Left] = mouse.left;
  io.MouseDown[ImGuiMouseButton_Right] = mouse.right;
  io.MouseDown[ImGuiMouseButton_Middle] = mouse.middle;
  io.DeltaTime =
      static_cast<float>(absl::ToDoubleSeconds(pilot_->GetDeltaTime()));

  // ImGui loads the ini file later in the first frame, and saves it in later
  // ones and at shutdown, all only while it has a name.
  io.IniFilename = nullptr;
  items_.clear();
}

void ImGuiPilotHooks::AddItem(ImGuiID id, const char* label) {
  // An item that reports its label is the last item, which holds its
  // rectangle.
  if (context_->LastItemData.ID != id) {
    return;
  }
  const std::string_view shown_label = GetShownText(label);
  if (shown_label.empty()) {
    return;
  }

  // Only what isn't clipped can be clicked.
  const ImGuiWindow* window = context_->CurrentWindow;
  ImRect rect = context_->LastItemData.Rect;
  rect.ClipWithFull(window->ClipRect);
  if (rect.GetWidth() <= 0 || rect.GetHeight() <= 0) {
    return;
  }
  const ImVec2 center = rect.GetCenter();
  items_.push_back(
      {.window_id = window->ID,
       .root_window = std::string(GetShownText(window->RootWindow->Name)),
       .label = std::string(shown_label),
       .center = {.x = static_cast<int>(std::floor(center.x)),
                  .y = static_cast<int>(std::floor(center.y))}});
}

bool ImGuiPilotHooks::HasLabel(const Item& item, std::string_view label) {
  if (item.label == label) {
    return true;
  }
  return absl::ConsumePrefix(&label, item.root_window) &&
         absl::ConsumePrefix(&label, "/") && label == item.label;
}

bool ImGuiPilotHooks::IsInFront(const Item& item) const {
  // Between frames, the windows are as the last frame left them, in the order
  // they are drawn. A window hidden in its first frame, as ImGui sizes it, is
  // never in front.
  const ImGuiWindow* window = FindFrontWindow(context_, item.center);
  return window != nullptr && window->ID == item.window_id;
}

std::vector<PilotPoint> ImGuiPilotHooks::FindItems(
    std::string_view label) const {
  std::vector<PilotPoint> centers;
  for (const Item& item : items_) {
    if (HasLabel(item, label) && IsInFront(item)) {
      centers.push_back(item.center);
    }
  }
  return centers;
}

}  // namespace internal
}  // namespace gb

//==============================================================================
// Test engine hooks
//
// ImGui calls these, as IMGUI_ENABLE_TEST_ENGINE is defined (in
// gb_imconfig.h), only for a context with TestEngineHookItems set, which
// ImGuiPilotHooks sets.
//==============================================================================

void ImGuiTestEngineHook_ItemAdd(ImGuiContext* context, const ImRect& bb,
                                 ImGuiID id) {}

void ImGuiTestEngineHook_ItemInfo(ImGuiContext* context, ImGuiID id,
                                  const char* label,
                                  ImGuiItemStatusFlags flags) {
  static_cast<gb::internal::ImGuiPilotHooks*>(context->TestEngine)
      ->AddItem(id, label);
}

void ImGuiTestEngineHook_IdInfo(ImGuiContext* context, ImGuiDataType data_type,
                                ImGuiID id, const void* data_id) {}

void ImGuiTestEngineHook_IdInfo(ImGuiContext* context, ImGuiDataType data_type,
                                ImGuiID id, const void* data_id,
                                const void* data_id_end) {}

void ImGuiTestEngineHook_Log(ImGuiContext* context, const char* fmt, ...) {}
