#ifndef GB_MESSAGE_MESSAGE_ENDPOINT_H_
#define GB_MESSAGE_MESSAGE_ENDPOINT_H_

#include <stdint.h>

#include <thread>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "gb/base/callback.h"
#include "gb/base/type_info.h"
#include "gb/base/weak_ptr.h"
#include "gb/message/message_types.h"

namespace gb {

//------------------------------------------------------------------------------
// MessageEndpointId
//------------------------------------------------------------------------------

// Messages are sent from and to message endpoints which is uniquely identified
// by a MessageEndpointId.
using MessageEndpointId = uint64_t;

// This represents the lack of a message endpoint.
//
// If messages are sent to kNoMessageEndpointId, then no endpoint will receive
// it. If messages are received by kNoMessageEndpointId, then it was sent
// directly from the MessageSystem class.
inline constexpr MessageEndpointId kNoMessageEndpointId = 0;

// Endpoint ID of the global broadcast channel.
//
// All message endpoints are implicitly subscribed to the broadcase message
// endpoint. It is possible for an endpoint to unsubscribe (and resubscribe) to
// this channel as desired.
inline constexpr MessageEndpointId kBroadcastMessageEndpointId = 1;

//------------------------------------------------------------------------------
// MessageHandler
//------------------------------------------------------------------------------

// Signature of a message handler callback.
template <typename Message>
using MessageHandler =
    Callback<void(MessageEndpointId from, const Message& message)>;

//------------------------------------------------------------------------------
// MessageEndpoint
//------------------------------------------------------------------------------

// This class represents an endpoint for both sending and receiving messages
// within a MessageSystem.
//
// This class is thread-safe.
class MessageEndpoint final {
 public:
  // Endpoints are not copiable or moveable.
  MessageEndpoint(const MessageEndpoint&) = delete;
  MessageEndpoint& operator=(const MessageEndpoint&) = delete;

  // Endpoint destructor. MessageEndpoints may not be deleted while any
  // registered handler of the endpoint is currently executing in the same
  // thread.
  ~MessageEndpoint();

  // Returns the message system associated with this endpoint.
  //
  // If this is null, then the underlying system was deleted, and this endpoint
  // is non-functional (no messages will be received or sent).
  const WeakPtr<MessageSystem>& GetSystem() const { return system_; }

  // The unique MessageEndpointId for this endpoint.
  //
  // No two endpoints in the same message system will will ever have the same
  // ID, and IDs are never recycled after an endpoint is destroyed.
  MessageEndpointId GetId() const { return id_; }

  // The name of this endpoint.
  //
  // Endpoint names are optional, and not necessarily unique. They should be
  // used for debugging or logging purposes only.
  const std::string& GetName() const { return name_; }

  // Subscribes to all messages sent to another endpoint (usually a channel).
  //
  // Any messages sent to the corresponding endpoint will be forwarded to this
  // endpoint as well. Returns true if the subscription is active, false if
  // the endpoint does not exist.
  //
  // It is valid to have circular subscriptions (each endpoint will only receive
  // a message once), and subscribing to the same endpoint more than once is
  // valid but has no effect (it trivially succeeds).
  bool Subscribe(MessageEndpointId endpoint);

  // Unsubscribes to messages seny to the specified endpoint (usually a
  // channel).
  //
  // Any messages sent to the corresponding endpoint will not be forwarded to
  // this endpoint. If there is no subscription to the endpoint (or the endpoint
  // doesn't exist), this does nothing. It is valid to unsubscribe from (and
  // later resubscribe to) the broadcast channel kBroadcastMessageEndpointId.
  void Unsubscribe(MessageEndpointId endpoint);

  // Returns true if this endpoint is currently subscribed to messages from the
  // other endpoint.
  bool IsSubscribed(MessageEndpointId endpoint) const;

  // Sets or clears a message handler for a specific type of message.
  //
  // If no handler is registered for a message when it is received, then that
  // message is dropped by the endpoint.
  template <typename Message>
  void SetHandler(MessageHandler<Message> callback);
  template <typename Message>
  void ClearHandler();

  // Sends a message from this endpoint to the specified endpoint.
  //
  // Messages are generally sent asynchronously, as determined by the
  // MessageDispatcher used for the receiving endpoint (see MessageDispatcher
  // for details). If the receiving endpoints do not have any dispatcher
  // defined, the message *may* get processed immediately within the Send call.
  // There is no guarantee, however -- if the receiving endpoint is already
  // handling a message, this message will be queued for later execution.
  //
  // Returns true if the message could be dispatched to the specified endpoint.
  // Note that this does not necessarily mean the endpoint will receive the
  // message, if the endpoint is deleted before the dispatch completes.
  template <typename Message>
  bool Send(MessageEndpointId to, const Message& message);

 public:
  // The following methods are internal to the message system, callable only by
  // other classes that are part of the system.

  // Constructs a new message endpoint.
  MessageEndpoint(MessageInternal, MessageSystem* system, MessageEndpointId id,
                  std::string_view name);

  // Receives a message from the message system.
  void Receive(MessageInternal, MessageEndpointId from, TypeInfo* type,
               const void* message);

 private:
  struct QueuedMessage {
    QueuedMessage(MessageEndpointId from, TypeInfo* type, void* message)
        : from(from), type(type), message(message) {}
    const MessageEndpointId from;
    TypeInfo* const type;
    void* const message;
  };
  using GenericHandler =
      Callback<void(MessageEndpointId from, const void* message)>;
  using Handlers = absl::flat_hash_map<TypeKey*, GenericHandler>;
  using QueuedMessages = std::vector<QueuedMessage>;

  bool DoSend(MessageEndpointId to, TypeInfo* type, const void* message);

  mutable absl::Mutex handler_mutex_;  // Guards access to the handlers.
  MessageEndpointId id_;
  std::string name_;
  WeakPtr<MessageSystem> system_;
  Handlers handlers_ ABSL_GUARDED_BY(handler_mutex_);
  bool calling_handler_ ABSL_GUARDED_BY(handler_mutex_) = false;
  std::thread::id calling_thread_ ABSL_GUARDED_BY(handler_mutex_);
  QueuedMessages queued_messages_ ABSL_GUARDED_BY(handler_mutex_);
};

//------------------------------------------------------------------------------
// Template / inline implementation
//------------------------------------------------------------------------------

template <typename Message>
void MessageEndpoint::SetHandler(MessageHandler<Message> callback) {
  absl::WriterMutexLock lock(&handler_mutex_);
  handlers_[TypeKey::Get<Message>()] = [callback = std::move(callback)](
                                           MessageEndpointId from,
                                           const void* message) {
    callback(from, *static_cast<const Message*>(message));
  };
}

template <typename Message>
void MessageEndpoint::ClearHandler() {
  absl::WriterMutexLock lock(&handler_mutex_);
  handlers_.erase(TypeKey::Get<Message>());
}

template <typename Message>
bool MessageEndpoint::Send(MessageEndpointId to, const Message& message) {
  return DoSend(to, TypeInfo::Get<Message>(), &message);
}

}  // namespace gb

#endif  // GB_MESSAGE_MESSAGE_ENDPOINT_H_
