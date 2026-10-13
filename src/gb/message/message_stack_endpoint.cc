// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/message/message_stack_endpoint.h"

#include <algorithm>

#include "absl/log/check.h"
#include "absl/memory/memory.h"

namespace gb {

bool MessageStackHandlers::SetStack(MessageInternal,
                                    MessageStackEndpoint* old_stack,
                                    MessageStackEndpoint* new_stack) {
  absl::MutexLock lock(&mutex_);
  if (stack_ != old_stack) {
    return false;
  }
  stack_ = new_stack;
  if (stack_ != nullptr) {
    for (auto& handler_info : handlers_) {
      handler_info.second.register_message();
    }
  }
  return true;
}

bool MessageStackHandlers::Receive(MessageInternal, MessageEndpointId from,
                                   TypeKey* key, const void* message) {
  absl::MutexLock lock(&mutex_);
  auto it = handlers_.find(key);
  if (it == handlers_.end()) {
    return false;
  }

  // The callback is called with the lock released, so it may set or clear
  // handlers. If it replaced or cleared itself, it is destroyed before the
  // lock is taken again.
  std::shared_ptr<Callback> callback = it->second.callback;
  mutex_.Unlock();
  bool result = (*callback)(from, message);
  callback.reset();
  mutex_.Lock();
  return result;
}

std::unique_ptr<MessageStackEndpoint> MessageStackEndpoint::Create(
    MessageSystem* message_system, MessageStackOrder default_order,
    std::string_view name) {
  return Create(message_system, default_order, nullptr, name);
}

std::unique_ptr<MessageStackEndpoint> MessageStackEndpoint::Create(
    MessageSystem* message_system, MessageStackOrder default_order,
    MessageDispatcher* dispatcher, std::string_view name) {
  if (message_system == nullptr) {
    return nullptr;
  }
  auto endpoint = message_system->CreateEndpoint(dispatcher, name);
  if (endpoint == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(
      new MessageStackEndpoint(default_order, std::move(endpoint)));
}

MessageStackEndpoint::MessageStackEndpoint(
    MessageStackOrder default_order, std::unique_ptr<MessageEndpoint> endpoint)
    : default_order_(default_order), endpoint_(std::move(endpoint)) {}

MessageStackEndpoint::~MessageStackEndpoint() {
  // The handlers are taken off the stack first, so they don't register any
  // more message types with the endpoint.
  std::vector<StackNode> stack;
  {
    absl::MutexLock lock(&mutex_);
    stack.swap(stack_);
  }
  for (const auto& node : stack) {
    if (node.cached_handlers == nullptr) {
      continue;
    }
    WeakLock<MessageStackHandlers> handlers_lock(&node.handlers);
    if (handlers_lock == nullptr) {
      continue;
    }
    handlers_lock->SetStack({}, this, nullptr);
  }

  // This waits for any handler that is running on another thread, and no more
  // messages are received after.
  endpoint_.reset();
}

bool MessageStackEndpoint::Push(MessageStackHandlers* handlers) {
  if (handlers == nullptr || !handlers->SetStack({}, nullptr, this)) {
    return false;
  }
  absl::MutexLock lock(&mutex_);
  stack_.emplace_back(handlers);
  return true;
}

bool MessageStackEndpoint::Remove(MessageStackHandlers* handlers) {
  if (handlers == nullptr || !handlers->SetStack({}, this, nullptr)) {
    return false;
  }
  absl::MutexLock lock(&mutex_);
  for (auto& node : stack_) {
    if (node.cached_handlers == handlers &&
        node.handlers.Lock().Get() == handlers) {
      node.handlers = nullptr;
      node.cached_handlers = nullptr;
      break;
    }
  }
  return true;
}

MessageStackEndpoint::Handlers MessageStackEndpoint::GetHandlers(TypeKey* key) {
  absl::MutexLock lock(&mutex_);
  auto message_it = messages_.find(key);
  DCHECK(message_it != messages_.end()) << "Unhandled message type";

  int begin, end, delta;
  if (message_it->second == MessageStackOrder::kTopDown) {
    begin = static_cast<int>(stack_.size()) - 1;
    end = -1;
    delta = -1;
  } else {
    begin = 0;
    end = static_cast<int>(stack_.size());
    delta = 1;
  }

  bool clean_stack = false;
  Handlers handlers;
  handlers.reserve(stack_.size());
  for (int i = begin; i != end; i += delta) {
    auto& node = stack_[i];
    if (node.cached_handlers == nullptr) {
      clean_stack = true;
      continue;
    }
    {
      WeakLock<MessageStackHandlers> handler_lock(&node.handlers);
      if (handler_lock == nullptr) {
        node.cached_handlers = nullptr;
        clean_stack = true;
        continue;
      }
    }
    handlers.emplace_back(node.handlers);
  }

  if (clean_stack) {
    stack_.erase(std::remove_if(stack_.begin(), stack_.end(),
                                [](const StackNode& node) {
                                  return node.cached_handlers == nullptr;
                                }),
                 stack_.end());
  }
  return handlers;
}

void MessageStackEndpoint::HandleMessage(MessageEndpointId from, TypeKey* key,
                                         const void* message) {
  for (auto& handlers : GetHandlers(key)) {
    WeakLock<MessageStackHandlers> handlers_lock(&handlers);
    if (handlers_lock != nullptr) {
      if (handlers_lock->Receive({}, from, key, message)) {
        break;
      }
    }
  }
}

}  // namespace gb
