// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/message/message_system.h"

#include "absl/memory/memory.h"
#include "gb/message/message_dispatcher.h"
#include "glog/logging.h"

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
    system->owned_system_dispatcher_ = std::move(owned_dispatcher);
    system->system_dispatcher_ = dispatcher;
    if (!dispatcher->SetSystem({}, system.get())) {
      system->system_dispatcher_ = nullptr;
      system->owned_system_dispatcher_.reset();
      return nullptr;
    }
    absl::WriterMutexLock lock(&system->mutex_);
    system->dispatchers_[dispatcher] = 1;
  }

  absl::WriterMutexLock lock(&system->mutex_);
  auto& endpoint_info = system->endpoints_[kBroadcastMessageEndpointId];
  endpoint_info = absl::WrapUnique(new EndpointInfo);
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
  absl::WriterMutexLock lock(&mutex_);
  if (dispatcher != nullptr) {
    if (!dispatcher->SetSystem({}, this)) {
      return nullptr;
    }
    ++dispatchers_[dispatcher];
  }
  auto endpoint_id = next_endpoint_id_++;
  auto endpoint =
      absl::WrapUnique(new MessageEndpoint({}, this, endpoint_id, name));
  auto& endpoint_info = endpoints_[endpoint_id];
  endpoint_info = absl::WrapUnique(new EndpointInfo);
  endpoint_info->name.assign(name.data(), name.size());
  endpoint_info->endpoint = endpoint.get();
  endpoint_info->dispatcher = dispatcher;

  endpoint_info->subscriptions.insert(kBroadcastMessageEndpointId);
  auto* broadcast_info = endpoints_[kBroadcastMessageEndpointId].get();
  if (broadcast_info->dispatch_threads.empty()) {
    broadcast_info->subscribers.insert(endpoint_id);
  } else {
    broadcast_info->add_subscribers.insert(endpoint_id);
  }

  return endpoint;
}

void MessageSystem::RemoveEndpoint(MessageInternal, MessageEndpoint* endpoint) {
  absl::WriterMutexLock lock(&mutex_);
  auto it = endpoints_.find(endpoint->GetId());
  auto* endpoint_info = it->second.get();
  if (!endpoint_info->dispatch_threads.empty()) {
    // If we are deleting the endpoint from a different thread, then wait for
    // the current dispatch to complete first.
    if (endpoint_info->dispatch_threads.find(std::this_thread::get_id()) ==
        endpoint_info->dispatch_threads.end()) {
      mutex_.Await(absl::Condition(
          +[](EndpointInfo* endpoint_info) {
            return endpoint_info->dispatch_threads.empty();
          },
          endpoint_info));
    } else {
      // We are erasing the endpoint on the same thread that is currently
      // dispatching to that endpoint. This is fine, as this code path only
      // occurs if it is a subscribed endpoint that is being called (self
      // deletion is caught in the MessageEndpoint destructor). To handle this
      // case, we just queue up the endpoint deregistration to be happen at the
      // end of the dispatch, and temporarily convert the endpoint to a message
      // channel.
      endpoint_info->endpoint = nullptr;
      endpoint_info->erase_after_dispatch = true;
    }
  }

  // It is safe to clear the dispatcher in all cases, as the endpoint is now
  // unreachable. Any queued messages will be discarded as the endpoint is gone.
  MessageDispatcher* dispatcher = nullptr;
  if (endpoint_info->dispatcher != nullptr) {
    if (--dispatchers_[endpoint_info->dispatcher] == 0) {
      dispatcher = endpoint_info->dispatcher;
      dispatchers_.erase(endpoint_info->dispatcher);
    }
  }

  if (!endpoint_info->erase_after_dispatch) {
    endpoints_.erase(it);
  }
}

MessageEndpointId MessageSystem::AddChannel(std::string_view name) {
  absl::WriterMutexLock lock(&mutex_);
  auto endpoint_id = next_endpoint_id_++;
  auto& endpoint_info = endpoints_[endpoint_id];
  endpoint_info = absl::WrapUnique(new EndpointInfo);
  endpoint_info->name.assign(name.data(), name.size());
  return endpoint_id;
}

bool MessageSystem::RemoveChannel(MessageEndpointId channel_id) {
  if (channel_id == kBroadcastMessageEndpointId) {
    return false;
  }
  absl::WriterMutexLock lock(&mutex_);
  auto it = endpoints_.find(channel_id);
  if (it == endpoints_.end()) {
    return false;
  }
  auto* endpoint_info = it->second.get();
  if (endpoint_info->endpoint != nullptr) {
    return false;
  }
  if (endpoint_info->dispatch_threads.empty()) {
    endpoints_.erase(it);
  } else if (endpoint_info->erase_after_dispatch) {
    return false;
  } else {
    endpoint_info->erase_after_dispatch = true;
  }
  return true;
}

MessageEndpointType MessageSystem::GetEndpointType(
    MessageEndpointId endpoint_id) {
  absl::ReaderMutexLock lock(&mutex_);
  auto it = endpoints_.find(endpoint_id);
  if (it == endpoints_.end()) {
    return MessageEndpointType::kInvalid;
  }
  const auto* endpoint_info = it->second.get();
  if (endpoint_info->erase_after_dispatch) {
    return MessageEndpointType::kInvalid;
  }
  return endpoint_info->endpoint != nullptr ? MessageEndpointType::kEndpoint
                                            : MessageEndpointType::kChannel;
}

bool MessageSystem::Subscribe(MessageInternal, MessageEndpointId source,
                              MessageEndpointId subscriber) {
  absl::WriterMutexLock lock(&mutex_);
  auto source_it = endpoints_.find(source);
  if (source_it == endpoints_.end()) {
    return false;
  }
  auto subscriber_it = endpoints_.find(subscriber);
  if (subscriber_it == endpoints_.end()) {
    return false;
  }
  auto* endpoint_info = subscriber_it->second.get();
  if (endpoint_info->subscriptions.find(source) !=
      endpoint_info->subscriptions.end()) {
    return true;
  }
  endpoint_info->subscriptions.insert(source);

  auto* source_info = source_it->second.get();
  if (source_info->dispatch_threads.empty()) {
    source_info->subscribers.insert(subscriber);
  } else if (auto remove_it = source_info->remove_subscribers.find(subscriber);
             remove_it != source_info->remove_subscribers.end()) {
    source_info->remove_subscribers.erase(remove_it);
  } else if (auto current_it = source_info->subscribers.find(subscriber);
             current_it == source_info->subscribers.end()) {
    source_info->add_subscribers.insert(subscriber);
  }
  return true;
}

void MessageSystem::Unsubscribe(MessageInternal, MessageEndpointId source,
                                MessageEndpointId subscriber) {
  absl::WriterMutexLock lock(&mutex_);
  auto subscriber_it = endpoints_.find(subscriber);
  if (subscriber_it == endpoints_.end()) {
    return;
  }
  auto* endpoint_info = subscriber_it->second.get();
  endpoint_info->subscriptions.erase(source);

  auto source_it = endpoints_.find(source);
  if (source_it == endpoints_.end()) {
    return;
  }
  auto* source_info = source_it->second.get();
  if (source_info->dispatch_threads.empty()) {
    source_info->subscribers.erase(subscriber);
  } else if (auto add_it = source_info->add_subscribers.find(subscriber);
             add_it != source_info->add_subscribers.end()) {
    source_info->add_subscribers.erase(add_it);
  } else if (auto current_it = source_info->subscribers.find(subscriber);
             current_it != source_info->subscribers.end()) {
    source_info->remove_subscribers.insert(subscriber);
  }
}

bool MessageSystem::IsSubscribed(MessageInternal, MessageEndpointId source,
                                 MessageEndpointId subscriber) {
  absl::WriterMutexLock lock(&mutex_);
  auto source_it = endpoints_.find(source);
  if (source_it == endpoints_.end()) {
    return false;
  }
  auto subscriber_it = endpoints_.find(subscriber);
  if (subscriber_it == endpoints_.end()) {
    return false;
  }
  auto* endpoint_info = subscriber_it->second.get();
  return endpoint_info->subscriptions.find(source) !=
         endpoint_info->subscriptions.end();
}

bool MessageSystem::DoSend(MessageInternal, MessageEndpointId from,
                           MessageEndpointId to, TypeInfo* type,
                           const void* message) {
  MessageDispatcher* dispatcher = nullptr;
  {
    absl::ReaderMutexLock lock(&mutex_);
    auto to_it = endpoints_.find(to);
    if (to_it == endpoints_.end()) {
      return false;
    }
    const auto* endpoint_info = to_it->second.get();
    dispatcher = system_dispatcher_;
    if (endpoint_info->dispatcher != nullptr) {
      dispatcher = endpoint_info->dispatcher;
    }
  }

  // Only copiable types can be sent as messages.
  if (!type->CanClone()) {
    return false;
  }
  if (dispatcher != nullptr) {
    dispatcher->AddMessage({}, {from, to, type, type->Clone(message)});
  } else {
    EndpointIdSet visited;
    // const_cast is ok here as delete_message is false.
    DispatchImpl(&visited, nullptr, from, to, type, const_cast<void*>(message),
                 false);
  }
  return true;
}

bool MessageSystem::DispatchImpl(EndpointIdSet* visited,
                                 MessageDispatcher* dispatcher,
                                 MessageEndpointId from, MessageEndpointId to,
                                 TypeInfo* type, void* message,
                                 bool delete_message) {
  if (visited->find(to) != visited->end()) {
    return true;
  }
  visited->insert(to);

  EndpointInfo* to_info = nullptr;
  {
    mutex_.WriterLock();
    auto to_it = endpoints_.find(to);
    if (to_it == endpoints_.end() || to_it->second->erase_after_dispatch) {
      mutex_.WriterUnlock();
      if (delete_message) {
        type->Destroy(message);
      }
      return false;
    }
    to_info = to_it->second.get();
    if (to_info->dispatcher != dispatcher && to_info->dispatcher != nullptr) {
      auto* dispatcher = to_info->dispatcher;
      mutex_.WriterUnlock();
      dispatcher->AddMessage({}, {from, to, type, type->Clone(message)});
      return true;
    }
    to_info->dispatch_threads.insert(std::this_thread::get_id());
    mutex_.WriterUnlock();
  }

  if (to_info->endpoint != nullptr) {
    to_info->endpoint->Receive({}, from, type, message);
  }
  EndpointIdSet deleted_endpoints;
  for (auto endpoint_id : to_info->subscribers) {
    if (!DispatchImpl(visited, dispatcher, from, endpoint_id, type, message,
                      false)) {
      deleted_endpoints.insert(endpoint_id);
    }
  }

  {
    absl::WriterMutexLock lock(&mutex_);
    for (auto endpoint_id : deleted_endpoints) {
      to_info->subscribers.erase(endpoint_id);
    }
    to_info->dispatch_threads.erase(std::this_thread::get_id());
    if (to_info->dispatch_threads.empty()) {
      if (to_info->erase_after_dispatch) {
        endpoints_.erase(to);
      } else {
        for (auto endpoint_id : to_info->add_subscribers) {
          to_info->subscribers.insert(endpoint_id);
        }
        to_info->add_subscribers.clear();
        for (auto endpoint_id : to_info->remove_subscribers) {
          to_info->subscribers.erase(endpoint_id);
        }
        to_info->remove_subscribers.clear();
      }
    }
  }

  if (delete_message) {
    type->Destroy(message);
  }
  return true;
}

}  // namespace gb
