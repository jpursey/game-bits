// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/pilot.h"

#include <algorithm>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"

namespace gb {

//==============================================================================
// PilotHandle
//==============================================================================

PilotHandle::PilotHandle(PilotHandle&& other)
    : pilot_(std::exchange(other.pilot_, nullptr)), id_(other.id_) {}

PilotHandle& PilotHandle::operator=(PilotHandle&& other) {
  if (this != &other) {
    Remove();
    pilot_ = std::exchange(other.pilot_, nullptr);
    id_ = other.id_;
  }
  return *this;
}

PilotHandle::~PilotHandle() { Remove(); }

void PilotHandle::Remove() {
  if (pilot_ != nullptr) {
    std::exchange(pilot_, nullptr)->Remove(id_);
  }
}

//==============================================================================
// Pilot
//==============================================================================

Pilot::~Pilot() {
  DCHECK(commands_.empty() && conditions_.empty() && item_finders_.empty())
      << "A PilotHandle outlived its pilot.";
}

template <typename Callback>
std::shared_ptr<Callback> Pilot::Find(
    const std::vector<Registration<Callback>>& registrations,
    std::string_view name) {
  for (const auto& registration : registrations) {
    if (registration.name == name) {
      return registration.callback;
    }
  }
  return nullptr;
}

PilotHandle Pilot::AddCommand(std::string_view name, Command command) {
  if (Find(commands_, name) != nullptr) {
    LOG(ERROR) << "Pilot command \"" << name << "\" is already registered.";
    return {};
  }
  const int id = next_id_++;
  commands_.push_back(
      {id, std::string(name), std::make_shared<Command>(std::move(command))});
  return PilotHandle(this, id);
}

PilotHandle Pilot::AddCondition(std::string_view name, Condition condition) {
  if (Find(conditions_, name) != nullptr) {
    LOG(ERROR) << "Pilot condition \"" << name << "\" is already registered.";
    return {};
  }
  const int id = next_id_++;
  conditions_.push_back({id, std::string(name),
                         std::make_shared<Condition>(std::move(condition))});
  return PilotHandle(this, id);
}

PilotHandle Pilot::AddItemFinder(ItemFinder finder) {
  const int id = next_id_++;
  item_finders_.push_back(
      {id, std::string(), std::make_shared<ItemFinder>(std::move(finder))});
  return PilotHandle(this, id);
}

void Pilot::BeginFrame() {
  DCHECK(!in_frame_) << "BeginFrame() called again before EndFrame().";
  in_frame_ = true;
  frame_saved_ = false;
  DoBeginFrame();
}

void Pilot::EndFrame() {
  DCHECK(in_frame_) << "EndFrame() called without BeginFrame().";
  in_frame_ = false;
  DoEndFrame();
  ++frame_;
}

void Pilot::SaveFrame(const Image& image) {
  DCHECK(in_frame_) << "SaveFrame() called outside a frame.";
  DCHECK(IsSavingFrame()) << "SaveFrame() called for a frame not being saved.";
  DCHECK(!frame_saved_) << "SaveFrame() called again in the same frame.";
  frame_saved_ = true;
  DoSaveFrame(image);
}

absl::Status Pilot::RunCommand(std::string_view name,
                               absl::Span<const std::string> args) {
  std::shared_ptr<Command> command = Find(commands_, name);
  if (command == nullptr) {
    return absl::NotFoundError(absl::StrCat("No command named \"", name, "\""));
  }
  return (*command)(args);
}

absl::StatusOr<bool> Pilot::CheckCondition(std::string_view name) {
  std::shared_ptr<Condition> condition = Find(conditions_, name);
  if (condition == nullptr) {
    return absl::NotFoundError(
        absl::StrCat("No condition named \"", name, "\""));
  }
  return (*condition)();
}

absl::StatusOr<PilotPoint> Pilot::FindItem(std::string_view label) {
  // A finder may add or remove finders, so the finders to ask are taken first,
  // and each is looked up again before it is asked, in case it was removed.
  std::vector<int> ids;
  ids.reserve(item_finders_.size());
  for (const auto& registration : item_finders_) {
    ids.push_back(registration.id);
  }

  std::vector<PilotPoint> found;
  for (int id : ids) {
    auto it = std::find_if(
        item_finders_.begin(), item_finders_.end(),
        [id](const auto& registration) { return registration.id == id; });
    if (it == item_finders_.end()) {
      continue;
    }
    std::shared_ptr<ItemFinder> finder = it->callback;
    std::vector<PilotPoint> points = (*finder)(label);
    found.insert(found.end(), points.begin(), points.end());
  }
  if (found.empty()) {
    return absl::NotFoundError(
        absl::StrCat("No item is labelled \"", label, "\""));
  }
  const int count = static_cast<int>(found.size());
  if (count > 1) {
    return absl::InvalidArgumentError(
        absl::StrCat(count, " items are labelled \"", label, "\""));
  }
  return found.front();
}

template <typename Callback>
std::shared_ptr<void> Pilot::Take(
    std::vector<Registration<Callback>>& registrations, int id) {
  auto it = std::find_if(
      registrations.begin(), registrations.end(),
      [id](const auto& registration) { return registration.id == id; });
  if (it == registrations.end()) {
    return nullptr;
  }
  std::shared_ptr<void> callback = std::move(it->callback);
  registrations.erase(it);
  return callback;
}

void Pilot::Remove(int id) {
  // The callback is destroyed only once it is out of its registry, as what it
  // holds may remove other registrations.
  std::shared_ptr<void> callback = Take(commands_, id);
  if (callback == nullptr) {
    callback = Take(conditions_, id);
  }
  if (callback == nullptr) {
    callback = Take(item_finders_, id);
  }
}

}  // namespace gb
