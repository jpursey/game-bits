// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_MESSAGE_DISPATCHER_H_
#define GB_BASE_MESSAGE_DISPATCHER_H_

#include <thread>

#include "absl/synchronization/mutex.h"
#include "gb/base/weak_ptr.h"
#include "gb/message/message_endpoint.h"

namespace gb {

//------------------------------------------------------------------------------
// MessageDispatcher
//------------------------------------------------------------------------------

// This base class represents an algorithm used to asynchronously dispatch
// messages sent from one endpoint to another.
//
// MessageDispatcher instances may be used for an entire MessageSystem
// (specifying the default message dispatching behavior), and can be specialized
// for use with specific MessageEndpoints. An endpoint dispatcher is always used
// in preference to the MessageSystem dispatcher.
//
// This class is thread-safe.
class MessageDispatcher {
 public:
  MessageDispatcher(const MessageDispatcher&) = delete;
  MessageDispatcher& operator=(const MessageDispatcher&) = delete;
  virtual ~MessageDispatcher();

  const WeakPtr<MessageSystem>& GetSystem() const { return system_; }

 public:
  // This struct describes a message in transit.
  struct Message {
    MessageEndpointId from = kNoMessageEndpointId;
    MessageEndpointId to = kNoMessageEndpointId;
    TypeInfo* type = nullptr;
    void* message = nullptr;
  };

  // Adds a message to the dispatcher.
  //
  // Derived classes must override this to define the behavior of when the
  // message is actually dispatched. When the derived class is ready to dispatch
  // the message to all receiving endpoints, it must call Dispatch with the
  // message. After calling Dispatch the message is invalid and must be
  // discarded.
  virtual void AddMessage(MessageInternal, const Message& message) = 0;

  // Updates the internal system pointer.
  //
  // A dispatcher can only be associated with one system. "new_system" cannot be
  // null.
  bool SetSystem(MessageInternal, MessageSystem* new_system);

 protected:
  // Default constructor, callable only by derived classes.
  MessageDispatcher() = default;

  // Called by derived classes to dispatch the message to all receiving
  // endpoints. "message" is considered invalid after this call complete. See
  // AddMessage for more details.
  void Dispatch(const Message& message);

 private:
  WeakPtr<MessageSystem> system_;
};

//------------------------------------------------------------------------------
// PollingMessageDispatcher
//------------------------------------------------------------------------------

// The PollingMessageDispatcher queues all messages until Update is called on
// the dispatcher.
//
// This is the safest (but potentially slowest) dispatcher, as the calling code
// can execute all queued callbacks at a known point in time. Handlers are free
// to use the message system in any way they like (short of deleting the
// MessageSystem instance or their own endpoint), as long as Update is called
// from outside of a handler (for instance, in the main game loop).
//
// For single-threaded applications this is generally the best choice.
class PollingMessageDispatcher : public MessageDispatcher {
 public:
  PollingMessageDispatcher() = default;
  ~PollingMessageDispatcher() override;

  // Dispatch all queues messages since the last time Update was called.
  void Update();

 public:
  void AddMessage(MessageInternal, const Message& message) override;

 private:
  mutable absl::Mutex mutex_;
  std::vector<Message> messages_ ABSL_GUARDED_BY(mutex_);
};

//------------------------------------------------------------------------------
// ThreadMessageDispatcher
//------------------------------------------------------------------------------

// The ThreadMessageDispatcher processes messages as soon as they are ready from
// a separate worker thread.
//
// Like the polling This dispatcher ensures that handlers are free to use
// message system in any way they like (short of deleting the MessageSystem
// instance or their own endpoint). Depending on scheduler pressure, it also may
// be faster, as the worker thread is notified as soon as a new message arrives.
// However, this dispatcher does require that all handlers that are called be
// thread-safe.
class ThreadMessageDispatcher : public MessageDispatcher {
 public:
  ThreadMessageDispatcher();
  ~ThreadMessageDispatcher() override;

  // Cancel dispatch thread, dispatching any remaining queued messages.
  //
  // No messages will be dispatched after this is called. This should be called
  // before any associated MessageSystem is destructed if there is any chance
  // that queued messages could exist at that time.
  void Cancel();

 public:
  void AddMessage(MessageInternal, const Message& message) override;

 private:
  bool ProcessMessagesReady() ABSL_SHARED_LOCKS_REQUIRED(thread_mutex_);
  void ProcessMessages();

  std::thread thread_;
  mutable absl::Mutex thread_mutex_;
  bool exit_thread_ ABSL_GUARDED_BY(thread_mutex_) = false;
  std::vector<Message> messages_ ABSL_GUARDED_BY(thread_mutex_);
};

}  // namespace gb

#endif  // GB_BASE_MESSAGE_DISPATCHER_H_
