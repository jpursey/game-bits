// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/message/message_endpoint.h"

#include "absl/log/log.h"
#include "gb/message/message_system.h"

namespace gb {

MessageEndpoint::MessageEndpoint(MessageInternal, MessageSystem* system,
                                 MessageEndpointId id, std::string_view name)
    : system_(system), id_(id), name_(name) {}

MessageEndpoint::~MessageEndpoint() {
  {
    absl::WriterMutexLock lock(&handler_mutex_);
    if (calling_handler_) {
      CHECK(calling_thread_ != std::this_thread::get_id())
          << "Deleting endpoint within its own handler.";
      handler_mutex_.Await(absl::Condition(
          +[](bool* calling_handler) { return !*calling_handler; },
          &calling_handler_));
    }
    handlers_.clear();
  }
  {
    WeakLock<MessageSystem> system(&system_);
    if (system == nullptr) {
      return;
    }
    system->RemoveEndpoint({}, this);
  }
}

bool MessageEndpoint::Subscribe(MessageEndpointId endpoint) {
  WeakLock<MessageSystem> system(&system_);
  if (system == nullptr) {
    return false;
  }
  return system->Subscribe({}, endpoint, id_);
}

void MessageEndpoint::Unsubscribe(MessageEndpointId endpoint) {
  WeakLock<MessageSystem> system(&system_);
  if (system == nullptr) {
    return;
  }
  system->Unsubscribe({}, endpoint, id_);
}

bool MessageEndpoint::IsSubscribed(MessageEndpointId endpoint) const {
  WeakLock<MessageSystem> system(&system_);
  if (system == nullptr) {
    return false;
  }
  return system->IsSubscribed({}, endpoint, id_);
}

void MessageEndpoint::Receive(MessageInternal, MessageEndpointId from,
                              TypeInfo* type, const void* message) {
  auto key = type->Key();
  int queue_index = 0;

  // Find the handler.
  absl::WriterMutexLock lock(&handler_mutex_);
  if (calling_handler_) {
    queued_messages_.emplace_back(from, type, type->Clone(message));
    return;
  }
  while (true) {
    auto it = handlers_.find(key);
    if (it == handlers_.end()) {
      return;
    }

    // In order to allow handlers to unregister themselves from within their
    // callback, we need to temporarily move the handler out of the map and
    // release the lock, to make the call.
    GenericHandler handler = std::move(it->second);
    calling_handler_ = true;
    calling_thread_ = std::this_thread::get_id();
    handler_mutex_.Unlock();
    handler(from, message);
    handler_mutex_.Lock();
    calling_handler_ = false;

    // Reset the handler in the map.
    it = handlers_.find(key);
    if (it != handlers_.end()) {
      it->second = std::move(handler);
    }

    if (queue_index >= static_cast<int>(queued_messages_.size())) {
      break;
    }

    const auto& queued_message = queued_messages_[queue_index++];
    from = queued_message.from;
    type = queued_message.type;
    key = type->Key();
    message = queued_message.message;
  }

  for (auto& queued_message : queued_messages_) {
    queued_message.type->Destroy(queued_message.message);
  }
  queued_messages_.clear();
}

bool MessageEndpoint::DoSend(MessageEndpointId to, TypeInfo* type,
                             const void* message) {
  WeakLock<MessageSystem> system(&system_);
  if (system == nullptr) {
    return false;
  }
  return system->DoSend({}, id_, to, type, message);
}

}  // namespace gb
