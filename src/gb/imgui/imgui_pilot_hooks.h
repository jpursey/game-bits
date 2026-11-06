// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMGUI_IMGUI_PILOT_HOOKS_H_
#define GB_IMGUI_IMGUI_PILOT_HOOKS_H_

#include <string>
#include <string_view>
#include <vector>

#include "gb/pilot/pilot.h"
#include "imgui.h"

namespace gb {
namespace internal {

// Connects an ImGui context to a pilot, for ImGuiInstance (see its
// kConstraintPilot), through ImGui's context hooks and test engine hooks.
//
// This class is thread-compatible.
class ImGuiPilotHooks final {
 public:
  // Adds the hooks to the context, and an item finder to the pilot, until this
  // is destroyed. The pilot and context must outlive this.
  ImGuiPilotHooks(Pilot* pilot, ImGuiContext* context);
  ImGuiPilotHooks(const ImGuiPilotHooks&) = delete;
  ImGuiPilotHooks& operator=(const ImGuiPilotHooks&) = delete;
  ~ImGuiPilotHooks();

  // Called by ImGui's hooks. BeginFrame() is called as ImGui::NewFrame()
  // starts, once the platform backend has set the mouse and delta time, and
  // replaces them with the pilot's, and clears the ini file's name. AddItem()
  // is called for each item that reports its label, as it is submitted.
  void BeginFrame();
  void AddItem(ImGuiID id, const char* label);

 private:
  struct Item {
    ImGuiID window_id;
    std::string root_window;  // Its name as shown.
    std::string label;        // As shown.
    PilotPoint center;
  };

  // Returns whether the item has the label, alone or after its root window
  // and a "/".
  static bool HasLabel(const Item& item, std::string_view label);

  // Returns whether a click at the item's center goes to its window, rather
  // than to one in front of it.
  bool IsInFront(const Item& item) const;

  // Returns the centers of the items drawn in the last frame with the label
  // that can be clicked. The pilot only calls this between frames.
  std::vector<PilotPoint> FindItems(std::string_view label) const;

  Pilot* const pilot_;
  ImGuiContext* const context_;
  ImGuiID new_frame_hook_ = 0;
  std::vector<Item> items_;  // Of the last frame, once it has ended.

  // Last, so the finder is removed before the items it reads.
  PilotHandle item_finder_;
};

}  // namespace internal
}  // namespace gb

#endif  // GB_IMGUI_IMGUI_PILOT_HOOKS_H_
