// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/message/message_dispatcher.h"

#include "gb/message/message_system.h"
#include "glog/logging.h"

namespace gb {

//------------------------------------------------------------------------------
// MessageDispatcher
//------------------------------------------------------------------------------

MessageDispatcher::~MessageDispatcher() {
  CHECK_EQ(system_.Lock().Get(), nullptr)
      << "MessageDispatcher is getting destructed while still in use by a "
         "MessageSystem.";
}

bool MessageDispatcher::SetSystem(MessageInternal, MessageSystem* new_system) {
  WeakLock<MessageSystem> system(&system_);
  if (new_system == nullptr || system != nullptr) {
    return system.Get() == new_system;
  }
  system_ = new_system;
  return true;
}

void MessageDispatcher::Dispatch(const Message& message) {
  WeakLock<MessageSystem> system(&system_);
  if (system == nullptr) {
    message.type->Destroy(message.message);
    return;
  }
  system->DoDispatch({}, this, message.from, message.to, message.type,
                     message.message);
}

//------------------------------------------------------------------------------
// PollingMessageDispatcher
//------------------------------------------------------------------------------

PollingMessageDispatcher::~PollingMessageDispatcher() {
  std::vector<Message> messages;
  {
    absl::MutexLock lock(&mutex_);
    messages.swap(messages_);
  }
  for (const auto& message : messages) {
    message.type->Destroy(message.message);
  }
}

void PollingMessageDispatcher::AddMessage(MessageInternal,
                                          const Message& message) {
  absl::MutexLock lock(&mutex_);
  messages_.emplace_back(message);
}

void PollingMessageDispatcher::Update() {
  absl::MutexLock lock(&mutex_);
  while (!messages_.empty()) {
    std::vector<Message> messages;
    messages.swap(messages_);
    mutex_.Unlock();
    for (const auto& message : messages) {
      Dispatch(message);
    }
    mutex_.Lock();
  }
}

//------------------------------------------------------------------------------
// ThreadMessageDispatcher
//------------------------------------------------------------------------------

ThreadMessageDispatcher::ThreadMessageDispatcher() {
  thread_ = std::thread([this]() { ProcessMessages(); });
}

ThreadMessageDispatcher::~ThreadMessageDispatcher() {
  {
    WeakLock<MessageSystem> system(&GetSystem());
    absl::MutexLock lock(&thread_mutex_);
    LOG_IF(WARNING, system != nullptr && !exit_thread_)
        << "ThreadMessageDispatcher was still running and associated with a "
           "MessageSystem at destruction. If messages are queued for "
           "processing, this will result in undefined behavior (likely a "
           "crash).";
  }
  Cancel();
}

void ThreadMessageDispatcher::Cancel() {
  CHECK(std::this_thread::get_id() != thread_.get_id())
      << "Cannot cancel ThreadDispatcher from within its own handlers.";
  {
    absl::MutexLock lock(&thread_mutex_);
    if (exit_thread_) {
      return;
    }
    exit_thread_ = true;
  }
  thread_.join();
  std::vector<Message> messages;
  {
    absl::MutexLock lock(&thread_mutex_);
    messages.swap(messages_);
  }
  for (const auto& message : messages) {
    Dispatch(message);
  }
}

void ThreadMessageDispatcher::AddMessage(MessageInternal,
                                         const Message& message) {
  absl::MutexLock lock(&thread_mutex_);
  if (exit_thread_) {
    message.type->Destroy(message.message);
    return;
  }
  messages_.emplace_back(message);
}

void ThreadMessageDispatcher::ProcessMessages() {
  absl::MutexLock lock(&thread_mutex_);
  while (!exit_thread_) {
    thread_mutex_.Await(absl::Condition(
        +[](ThreadMessageDispatcher* self) {
          return self->exit_thread_ || !self->messages_.empty();
        },
        this));

    std::vector<Message> messages;
    messages.swap(messages_);

    thread_mutex_.Unlock();
    for (const auto& message : messages) {
      Dispatch(message);
    }
    thread_mutex_.Lock();
  }
}

}  // namespace gb
