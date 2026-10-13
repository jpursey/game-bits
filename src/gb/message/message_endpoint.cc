// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/message/message_endpoint.h"

#include <memory>
#include <thread>

#include "absl/log/check.h"
#include "gb/message/message_system.h"

namespace gb {

MessageEndpoint::MessageEndpoint(MessageInternal, MessageSystem* system,
                                 MessageEndpointId id, std::string_view name)
    : weak_scope_(this), system_(system), id_(id), name_(name) {}

MessageEndpoint::~MessageEndpoint() {
  {
    absl::MutexLock lock(&handler_mutex_);
    CHECK(!calling_handler_ || calling_thread_ != std::this_thread::get_id())
        << "Deleting endpoint within its own handler.";
  }

  // This waits for messages being delivered on other threads (including any
  // handler that is running), and no more are delivered after.
  weak_scope_.InvalidateWeakPtrs();

  WeakLock<MessageSystem> system(&system_);
  if (system != nullptr) {
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
  absl::WriterMutexLock lock(&handler_mutex_);
  if (calling_handler_) {
    queued_messages_.emplace_back(from, type, type->Clone(message));
    return;
  }
  calling_thread_ = std::this_thread::get_id();
  CallHandler(from, type, message);

  // Messages received while a handler was running, on this thread or others.
  // The queue may grow while each one is handled, so each is copied out.
  for (int i = 0; i < static_cast<int>(queued_messages_.size()); ++i) {
    const QueuedMessage queued_message = queued_messages_[i];
    CallHandler(queued_message.from, queued_message.type,
                queued_message.message);
    queued_message.type->Destroy(queued_message.message);
  }
  queued_messages_.clear();
}

void MessageEndpoint::CallHandler(MessageEndpointId from, TypeInfo* type,
                                  const void* message) {
  auto it = handlers_.find(type->Key());
  if (it == handlers_.end()) {
    return;
  }

  // The handler is called with the lock released, so it may set or clear
  // handlers. If it replaced or cleared itself, it is destroyed before the
  // lock is taken again.
  std::shared_ptr<GenericHandler> handler = it->second;
  calling_handler_ = true;
  handler_mutex_.Unlock();
  (*handler)(from, message);
  handler.reset();
  handler_mutex_.Lock();
  calling_handler_ = false;
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
