// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_MESSAGE_MESSAGE_SYSTEM_H_
#define GB_MESSAGE_MESSAGE_SYSTEM_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "gb/base/type_info.h"
#include "gb/base/weak_ptr.h"
#include "gb/message/message_endpoint.h"
#include "gb/message/message_types.h"

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
  // The message is received by the endpoint, and every endpoint subscribed to
  // it (directly, or through other subscriptions), once each. Each endpoint
  // receives it through its own dispatcher, or the system dispatcher if it has
  // none. An endpoint that is no longer subscribed by the time its dispatcher
  // delivers the message doesn't receive it, and one that subscribes after the
  // message is sent may or may not.
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

  // Called from a dispatcher to deliver a message it was given to the
  // endpoints that use it, and then destroy the message.
  void DoDispatch(MessageInternal, MessageDispatcher* dispatcher,
                  MessageEndpointId from, MessageEndpointId to, TypeInfo* type,
                  void* message);

 private:
  using EndpointIdSet = absl::flat_hash_set<MessageEndpointId>;
  struct EndpointInfo {
    std::string name;

    // True for a channel, which has no endpoint.
    bool is_channel = true;

    // The endpoint, which is invalidated as it is destroyed.
    WeakPtr<MessageEndpoint> endpoint;

    // The dispatcher that delivers messages to the endpoint: its own, or the
    // system dispatcher. If this is null, messages are delivered immediately.
    MessageDispatcher* dispatcher = nullptr;

    EndpointIdSet subscribers;    // Endpoints subscribed to this endpoint.
    EndpointIdSet subscriptions;  // Endpoints this endpoint is subscribed to.
  };
  using Endpoints =
      absl::flat_hash_map<MessageEndpointId, std::unique_ptr<EndpointInfo>>;
  using Receivers = std::vector<WeakPtr<MessageEndpoint>>;

  static std::unique_ptr<MessageSystem> DoCreate(
      std::unique_ptr<MessageDispatcher> owned_dispatcher,
      MessageDispatcher* dispatcher);

  MessageSystem();

  // Returns the endpoints (not channels) that receive a message sent to "to":
  // "to" itself, and everything subscribed to it, directly or through other
  // subscriptions.
  std::vector<const EndpointInfo*> FindReceivers(MessageEndpointId to) const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  // Delivers a message to each receiver, in turn. Receivers that have been
  // destroyed are skipped.
  static void DeliverTo(const Receivers& receivers, MessageEndpointId from,
                        TypeInfo* type, const void* message);

  // Removes an endpoint or channel, and all subscriptions to or from it.
  void EraseEndpoint(Endpoints::iterator it)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  std::unique_ptr<MessageDispatcher> owned_system_dispatcher_;
  MessageDispatcher* system_dispatcher_ = nullptr;
  int64_t next_endpoint_id_ ABSL_GUARDED_BY(mutex_) =
      2;  // 0 and 1 are reserved.
  Endpoints endpoints_ ABSL_GUARDED_BY(mutex_);
};

template <typename Message>
bool MessageSystem::Send(MessageEndpointId to, const Message& message) {
  return DoSend({}, kNoMessageEndpointId, to, TypeInfo::Get<Message>(),
                &message);
}

}  // namespace gb

#endif  // GB_MESSAGE_MESSAGE_SYSTEM_H_
