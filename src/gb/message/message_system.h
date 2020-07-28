#ifndef GB_BASE_MESSAGE_MESSAGE_SYSTEM_H_
#define GB_BASE_MESSAGE_MESSAGE_SYSTEM_H_

#include <memory>
#include <string_view>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "gb/base/type_info.h"
#include "gb/base/weak_ptr.h"
#include "gb/message/message_endpoint.h"

namespace gb {

// Specifies the type of endpoint for a given endpoint ID.
enum class MessageEndpointType : int {
  kInvalid,
  kEndpoint,
  kChannel,
};

// This class manages a set of message senders and receivers with support for
// synchronous or asynchronous message delivery.
//
// Messages are sent and received via MessageEndpoint, and any copiable type can
// be sent or received as a message. Further, message channels can be created to
// allow sending to all endpoints which subscribe to the channel.
//
// This class is thread-safe.
class MessageSystem final : public WeakScope<MessageSystem> {
 public:
  ~MessageSystem();
  MessageSystem(const MessageSystem&) = delete;
  MessageSystem& operator=(const MessageSystem&) = delete;

  // Creates a new message system.
  //
  // An optional message dispatcher may be provided that will be used by default
  // if no endpoint-specific dispatcher overrides it. If a dispatcher is passed
  // in, it must remain valid for the lifetime of the returned MessageSystem
  // instance. If no dispatcher is passed in, then messages will be sent
  // immediately to all endpoints (before the Send call returns).
  //
  // It is _highly_ recommended that a dispatcher be used by default, as that
  // eliminates or reduces re-entrant code, giving message handlers greater
  // flexibility in the types of actions they can take.
  static std::unique_ptr<MessageSystem> Create(
      MessageDispatcher* dispatcher = nullptr);
  static std::unique_ptr<MessageSystem> Create(
      std::unique_ptr<MessageDispatcher> dispatcher);

  // Creates a new unique message endpoint which can send or receive messages.
  //
  // All endpoints have an optional name, that may be used for debugging.
  // Multiple endpoints may have the same name.
  //
  // The caller must call AddHandler to the endpoint to receive any messages.
  // Once the returned MessageEndpoint is destroyed, that endpoint ID is no
  // longer valid. All messages to it will be ignored.
  std::unique_ptr<MessageEndpoint> CreateEndpoint(std::string_view name = {});

  // Creates and endpoint with the specified dispatcher.
  //
  // The dispatcher will be used in preference to the system-wide dispatcher
  // when sending messages to this endpoint. The dispatcher must live longer
  // than all endpoints that use it. Passing nullptr for the dispatcher will
  // result in the system-wide dispatcher being used for this endpoint.
  std::unique_ptr<MessageEndpoint> CreateEndpoint(MessageDispatcher* dispatcher,
                                                  std::string_view name = {});

  // Adds a message channel which may be used to group related messages.
  //
  // A message channel is owned by the message system itself and can be used to
  // group messages together. Like endpoints, message channels may have an
  // optional name that may be used for debugging.
  MessageEndpointId AddChannel(std::string_view name = {});

  // Removes the specified message channel.
  //
  // Returns true if the specified channel_id was a channel and is now removed.
  // If this returns false, it is because the endpoint ID either did not exist,
  // was an endpoint, or was kBroadcastMessageEndpointId which cannot be
  // removed.
  bool RemoveChannel(MessageEndpointId channel_id);

  // Returns the endpoint type for the specified ID.
  //
  // If the endpoint_id is invalid, this will return
  // MessageEndpointType::kInvalid.
  MessageEndpointType GetEndpointType(MessageEndpointId endpoint_id);

  // Returns true if the endpoint ID is valid to send to.
  bool IsValidEndpoint(MessageEndpointId endpoint_id) {
    return GetEndpointType(endpoint_id) != MessageEndpointType::kInvalid;
  }

  // Send an anonymous message to the specified endpoint.
  //
  // The resulting endpoint(s) that receive the message will have
  // kNoMessageEndpointId as the originator of the message.
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

  // Called by the MessageEndpoint destructor to unregister the endpoint from
  // the system.
  void RemoveEndpoint(MessageInternal, MessageEndpoint* endpoint);

  // Subscribe one endpoint to another endpoint.
  //
  // Any messages sent to "source" will be forwarded to "subscriber" as well.
  // Returns true if the subscription is active, false if either endpoint does
  // not exist.
  //
  // It is valid to have circular subscriptions (each endpoint will only receive
  // a message once), and subscribing to the same endpoint more than once is
  // valid but has no effect (it trivially succeeds).
  bool Subscribe(MessageInternal, MessageEndpointId source,
                 MessageEndpointId subscriber);

  // Unsubscribe one endpoint from another endpoint.
  //
  // Any messages sent to "source" will not be forwarded to "subscriber". If
  // there is no subscription currently (or neither endpoint exists), this does
  // nothing. It is valid to unsubscribe from the broadcast channel.
  void Unsubscribe(MessageInternal, MessageEndpointId source,
                   MessageEndpointId subscriber);

  // Returns true if "subscriber" is subscribed to "source".
  bool IsSubscribed(MessageInternal, MessageEndpointId source,
                    MessageEndpointId subscriber);

  // Called from an endpoint to send a message. This will add the message to the
  // appropriate dispatcher, for immediate or deferred execution. Returns true
  // if the message could be dispatched to a known endpoint.
  bool DoSend(MessageInternal, MessageEndpointId from, MessageEndpointId to,
              TypeInfo* type, const void* message);

  // Called from a dispatcher to actually propagate the message to the specified
  // endpoint.
  void DoDispatch(MessageInternal, MessageDispatcher* dispatcher,
                  MessageEndpointId from, MessageEndpointId to, TypeInfo* type,
                  void* message) {
    EndpointIdSet visited;
    DispatchImpl(&visited, dispatcher, from, to, type, message, true);
  }

 private:
  using EndpointIdSet = absl::flat_hash_set<MessageEndpointId>;
  struct EndpointInfo {
    std::string name;
    MessageEndpoint* endpoint = nullptr;
    MessageDispatcher* dispatcher = nullptr;
    EndpointIdSet subscribers;    // Endpoints subscribed to this endpoint.
    EndpointIdSet subscriptions;  // Endpoints this endpoint is subscribed to.

    // While handlers are being dispatched, "subscribers" cannot be safely
    // changed, so modifications are queued up and applied at the end of the
    // dispatch.
    absl::flat_hash_set<std::thread::id> dispatch_threads;
    EndpointIdSet add_subscribers;
    EndpointIdSet remove_subscribers;
    bool erase_after_dispatch = false;
  };
  using Endpoints =
      absl::flat_hash_map<MessageEndpointId, std::unique_ptr<EndpointInfo>>;
  using Dispatchers = absl::flat_hash_map<MessageDispatcher*, int64_t>;

  static std::unique_ptr<MessageSystem> DoCreate(
      std::unique_ptr<MessageDispatcher> owned_dispatcher,
      MessageDispatcher* dispatcher);

  MessageSystem();

  // Returns false if the "to" endpoint no longer exists.
  bool DispatchImpl(EndpointIdSet* visited, MessageDispatcher* dispatcher,
                    MessageEndpointId from, MessageEndpointId to,
                    TypeInfo* type, void* message, bool delete_message);

  mutable absl::Mutex mutex_;
  std::unique_ptr<MessageDispatcher> owned_system_dispatcher_;
  MessageDispatcher* system_dispatcher_ = nullptr;
  int64_t next_endpoint_id_ ABSL_GUARDED_BY(mutex_) =
      2;  // 0 and 1 are reserved.
  Endpoints endpoints_ ABSL_GUARDED_BY(mutex_);
  Dispatchers dispatchers_ ABSL_GUARDED_BY(mutex_);
};

template <typename Message>
bool MessageSystem::Send(MessageEndpointId to, const Message& message) {
  return DoSend({}, kNoMessageEndpointId, to, TypeInfo::Get<Message>(),
                &message);
}

}  // namespace gb

#endif  // GB_BASE_MESSAGE_MESSAGE_SYSTEM_H_
