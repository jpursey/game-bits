// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/message/message_system.h"

#include <utility>

#include "absl/algorithm/container.h"
#include "absl/container/inlined_vector.h"
#include "absl/memory/memory.h"
#include "gb/message/message_dispatcher.h"

namespace gb {

std::unique_ptr<MessageSystem> MessageSystem::Create(
    MessageDispatcher* dispatcher) {
  return DoCreate(nullptr, dispatcher);
}

std::unique_ptr<MessageSystem> MessageSystem::Create(
    std::unique_ptr<MessageDispatcher> dispatcher) {
  return DoCreate(std::move(dispatcher), nullptr);
}

std::unique_ptr<MessageSystem> MessageSystem::DoCreate(
    std::unique_ptr<MessageDispatcher> owned_dispatcher,
    MessageDispatcher* dispatcher) {
  if (dispatcher == nullptr) {
    dispatcher = owned_dispatcher.get();
  }

  auto system = absl::WrapUnique(new MessageSystem);
  if (dispatcher != nullptr) {
    if (!dispatcher->SetSystem({}, system.get())) {
      return nullptr;
    }
    system->owned_system_dispatcher_ = std::move(owned_dispatcher);
    system->system_dispatcher_ = dispatcher;
  }

  absl::MutexLock lock(&system->mutex_);
  auto& endpoint_info = system->endpoints_[kBroadcastMessageEndpointId];
  endpoint_info = std::make_unique<EndpointInfo>();
  endpoint_info->name = "BroadcastChannel";
  return system;
}

MessageSystem::MessageSystem() : WeakScope<MessageSystem>(this) {}

MessageSystem::~MessageSystem() { InvalidateWeakPtrs(); }

std::unique_ptr<MessageEndpoint> MessageSystem::CreateEndpoint(
    std::string_view name) {
  return CreateEndpoint(nullptr, name);
}

std::unique_ptr<MessageEndpoint> MessageSystem::CreateEndpoint(
    MessageDispatcher* dispatcher, std::string_view name) {
  if (dispatcher == nullptr) {
    dispatcher = system_dispatcher_;
  } else if (!dispatcher->SetSystem({}, this)) {
    return nullptr;
  }

  absl::MutexLock lock(&mutex_);
  auto endpoint_id = next_endpoint_id_++;
  auto endpoint =
      absl::WrapUnique(new MessageEndpoint({}, this, endpoint_id, name));
  auto& endpoint_info = endpoints_[endpoint_id];
  endpoint_info = std::make_unique<EndpointInfo>();
  endpoint_info->name = std::string(name);
  endpoint_info->is_channel = false;
  endpoint_info->endpoint = endpoint->GetWeakPtr({});
  endpoint_info->dispatcher = dispatcher;
  endpoint_info->subscriptions.insert(kBroadcastMessageEndpointId);
  endpoints_[kBroadcastMessageEndpointId]->subscribers.insert(endpoint_id);
  return endpoint;
}

void MessageSystem::RemoveEndpoint(MessageInternal, MessageEndpoint* endpoint) {
  absl::MutexLock lock(&mutex_);
  EraseEndpoint(endpoints_.find(endpoint->GetId()));
}

MessageEndpointId MessageSystem::AddChannel(std::string_view name) {
  absl::MutexLock lock(&mutex_);
  auto endpoint_id = next_endpoint_id_++;
  auto& endpoint_info = endpoints_[endpoint_id];
  endpoint_info = std::make_unique<EndpointInfo>();
  endpoint_info->name = std::string(name);
  return endpoint_id;
}

bool MessageSystem::RemoveChannel(MessageEndpointId channel_id) {
  if (channel_id == kBroadcastMessageEndpointId) {
    return false;
  }
  absl::MutexLock lock(&mutex_);
  auto it = endpoints_.find(channel_id);
  if (it == endpoints_.end() || !it->second->is_channel) {
    return false;
  }
  EraseEndpoint(it);
  return true;
}

void MessageSystem::EraseEndpoint(Endpoints::iterator it) {
  const MessageEndpointId endpoint_id = it->first;
  const EndpointInfo* endpoint_info = it->second.get();
  for (MessageEndpointId source : endpoint_info->subscriptions) {
    if (auto source_it = endpoints_.find(source);
        source_it != endpoints_.end()) {
      source_it->second->subscribers.erase(endpoint_id);
    }
  }
  for (MessageEndpointId subscriber : endpoint_info->subscribers) {
    if (auto subscriber_it = endpoints_.find(subscriber);
        subscriber_it != endpoints_.end()) {
      subscriber_it->second->subscriptions.erase(endpoint_id);
    }
  }
  endpoints_.erase(it);
}

MessageEndpointType MessageSystem::GetEndpointType(
    MessageEndpointId endpoint_id) {
  absl::ReaderMutexLock lock(&mutex_);
  auto it = endpoints_.find(endpoint_id);
  if (it == endpoints_.end()) {
    return MessageEndpointType::kInvalid;
  }
  return it->second->is_channel ? MessageEndpointType::kChannel
                                : MessageEndpointType::kEndpoint;
}

bool MessageSystem::Subscribe(MessageInternal, MessageEndpointId source,
                              MessageEndpointId subscriber) {
  absl::MutexLock lock(&mutex_);
  auto source_it = endpoints_.find(source);
  if (source_it == endpoints_.end()) {
    return false;
  }
  auto subscriber_it = endpoints_.find(subscriber);
  if (subscriber_it == endpoints_.end()) {
    return false;
  }
  source_it->second->subscribers.insert(subscriber);
  subscriber_it->second->subscriptions.insert(source);
  return true;
}

void MessageSystem::Unsubscribe(MessageInternal, MessageEndpointId source,
                                MessageEndpointId subscriber) {
  absl::MutexLock lock(&mutex_);
  auto subscriber_it = endpoints_.find(subscriber);
  if (subscriber_it == endpoints_.end() ||
      subscriber_it->second->subscriptions.erase(source) == 0) {
    return;
  }
  if (auto source_it = endpoints_.find(source); source_it != endpoints_.end()) {
    source_it->second->subscribers.erase(subscriber);
  }
}

bool MessageSystem::IsSubscribed(MessageInternal, MessageEndpointId source,
                                 MessageEndpointId subscriber) {
  absl::ReaderMutexLock lock(&mutex_);
  auto subscriber_it = endpoints_.find(subscriber);
  return subscriber_it != endpoints_.end() &&
         subscriber_it->second->subscriptions.contains(source);
}

bool MessageSystem::DoSend(MessageInternal, MessageEndpointId from,
                           MessageEndpointId to, TypeInfo* type,
                           const void* message) {
  // Only copiable types can be sent as messages.
  if (!type->CanClone()) {
    return false;
  }

  // Each dispatcher that delivers to a receiver gets its own copy of the
  // message, and receivers without one get it now.
  absl::InlinedVector<MessageDispatcher*, 2> dispatchers;
  Receivers receivers;
  {
    absl::ReaderMutexLock lock(&mutex_);
    if (!endpoints_.contains(to)) {
      return false;
    }
    for (const EndpointInfo* receiver : FindReceivers(to)) {
      if (receiver->dispatcher == nullptr) {
        receivers.push_back(receiver->endpoint);
      } else if (!absl::c_linear_search(dispatchers, receiver->dispatcher)) {
        dispatchers.push_back(receiver->dispatcher);
      }
    }
  }

  for (MessageDispatcher* dispatcher : dispatchers) {
    dispatcher->AddMessage({}, {from, to, type, type->Clone(message)});
  }
  DeliverTo(receivers, from, type, message);
  return true;
}

void MessageSystem::DoDispatch(MessageInternal, MessageDispatcher* dispatcher,
                               MessageEndpointId from, MessageEndpointId to,
                               TypeInfo* type, void* message) {
  // The receivers are found again, as subscriptions may have changed since the
  // message was sent.
  Receivers receivers;
  {
    absl::ReaderMutexLock lock(&mutex_);
    for (const EndpointInfo* receiver : FindReceivers(to)) {
      if (receiver->dispatcher == dispatcher) {
        receivers.push_back(receiver->endpoint);
      }
    }
  }
  DeliverTo(receivers, from, type, message);
  type->Destroy(message);
}

std::vector<const MessageSystem::EndpointInfo*> MessageSystem::FindReceivers(
    MessageEndpointId to) const {
  std::vector<const EndpointInfo*> receivers;
  EndpointIdSet visited = {to};
  std::vector<MessageEndpointId> pending = {to};
  while (!pending.empty()) {
    const MessageEndpointId endpoint_id = pending.back();
    pending.pop_back();
    auto it = endpoints_.find(endpoint_id);
    if (it == endpoints_.end()) {
      continue;
    }
    const EndpointInfo* endpoint_info = it->second.get();
    if (!endpoint_info->is_channel) {
      receivers.push_back(endpoint_info);
    }
    for (MessageEndpointId subscriber : endpoint_info->subscribers) {
      if (visited.insert(subscriber).second) {
        pending.push_back(subscriber);
      }
    }
  }
  return receivers;
}

void MessageSystem::DeliverTo(const Receivers& receivers,
                              MessageEndpointId from, TypeInfo* type,
                              const void* message) {
  // An earlier receiver's handler may destroy a later one, and an endpoint
  // waits for this lock to be released before it is destroyed.
  for (const WeakPtr<MessageEndpoint>& receiver : receivers) {
    WeakLock<MessageEndpoint> endpoint(&receiver);
    if (endpoint != nullptr) {
      endpoint->Receive({}, from, type, message);
    }
  }
}

}  // namespace gb
