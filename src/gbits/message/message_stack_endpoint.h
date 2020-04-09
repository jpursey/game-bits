#ifndef GBITS_MESSAGE_MESSAGE_STACK_ENDPOINT_H_
#define GBITS_MESSAGE_MESSAGE_STACK_ENDPOINT_H_

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "gbits/base/weak_ptr.h"
#include "gbits/message/message_dispatcher.h"
#include "gbits/message/message_endpoint.h"
#include "gbits/message/message_system.h"

namespace gb {

//------------------------------------------------------------------------------
// MessageStackHandlers
//------------------------------------------------------------------------------

template <typename Message>
using MessageStackHandler =
    Callback<bool(MessageEndpointId from, const Message& message)>;

// This class manages a set of message handlers that can be pushed onto a
// MessageStackEndpoint.
//
// MessageStackHandlers can have a lifetime independent of the stack, and may be
// added and removed freely as needed. For instance, a set of handlers can be
// set up once, and then added to a stack only when needed.
//
// This class is thread-safe.
class MessageStackHandlers final : public WeakScope<MessageStackHandlers> {
 public:
  MessageStackHandlers() : WeakScope<MessageStackHandlers>(this) {}
  ~MessageStackHandlers() override { InvalidateWeakPtrs(); }

  // Returns the stack currently associated with these handlers, or nullptr if
  // it is not currently in a stack.
  MessageStackEndpoint* GetStack() const;

  // Sets or clears a message handler for a specific type of message.
  //
  // If no handler is registered for a message when it is received, then that
  // message is passed on to the next handlers in the stack.
  template <typename Message>
  void SetHandler(MessageStackHandler<Message> callback);
  template <typename Message>
  void ClearHandler();

 public:
  // The following methods are internal to the message system, callable only by
  // other classes that are part of the system.
  void SetStack(MessageInternal, MessageStackEndpoint* stack);
  bool Receive(MessageInternal, MessageEndpointId from, TypeKey* key,
               const void* message);

 private:
  struct HandlerInfo {
    Callback<bool(MessageEndpointId from, const void* message)> callback;
    Callback<void()> register_message;
  };
  using Handlers = absl::flat_hash_map<TypeKey*, HandlerInfo>;

  mutable absl::Mutex mutex_;
  MessageStackEndpoint* stack_ ABSL_GUARDED_BY(mutex_) = nullptr;
  Handlers handlers_ ABSL_GUARDED_BY(mutex_);
};

//------------------------------------------------------------------------------
// MessageStackEndpoint
//------------------------------------------------------------------------------

// Enumeration to specify what order a message should be processed in on a
// MessageStackEndpoint. This is configurable per message type.
enum class MessageStackOrder {
  kTopDown,
  kBottomUp,
};

// This class represents an endpoint that manages a stack of handlers instead of
// a single handler per message.
//
// When a message is received on this endpoint, it will be passed to handlers in
// turn on the stack based on the MessageStackOrder for that message type. If
// any handler returns true from its message handler, the message is considered
// handled, and no further handlers below (kTopDown) or above (kBottomUp) will
// be called.
//
// This class is thread-safe.
class MessageStackEndpoint final {
 public:
  // This class is neither copiable or movable.
  MessageStackEndpoint(const MessageStackEndpoint&) = delete;
  MessageStackEndpoint& operator=(const MessageStackEndpoint&) = delete;
  ~MessageStackEndpoint();

  // Constructs a stack endpoint from the specified system.
  static std::unique_ptr<MessageStackEndpoint> Create(
      MessageSystem* message_system, MessageStackOrder default_order,
      std::string_view name = {});
  static std::unique_ptr<MessageStackEndpoint> Create(
      MessageSystem* message_system, MessageStackOrder default_order,
      MessageDispatcher* dispatcher, std::string_view name = {});

  // Sets the order for the specified message type.
  template <typename Message>
  void SetOrder(MessageStackOrder order);

  // These functions mirror endpoint functions, and pass through to the
  // underlying endpoint.
  const WeakPtr<MessageSystem>& GetSystem() const {
    return endpoint_->GetSystem();
  }
  MessageEndpointId GetId() const { return endpoint_->GetId(); }
  const std::string& GetName() const { return endpoint_->GetName(); }
  bool Subscribe(MessageEndpointId endpoint) {
    return endpoint_->Subscribe(endpoint);
  }
  void Unsubscribe(MessageEndpointId endpoint) {
    endpoint_->Unsubscribe(endpoint);
  }
  bool IsSubscribed(MessageEndpointId endpoint) const {
    return endpoint_->IsSubscribed(endpoint);
  }
  template <typename Message>
  bool Send(MessageEndpointId to, const Message& message) {
    return endpoint_->Send(to, message);
  }

  // Pushes the handlers onto the stack.
  //
  // If the handlers are deleted, they are automatically removed from the stack.
  bool Push(MessageStackHandlers* handlers);

  // Explicitly removes the handlers from the stack.
  //
  // Handlers can be removed from anywhere in the stack.
  bool Remove(MessageStackHandlers* handlers);

 public:
  // The following methods are internal to the message system, callable only by
  // other classes that are part of the system.
  template <typename Message>
  void RegisterMessage(MessageInternal) {
    GetMessageInfo<Message>();
  }

 private:
  struct StackNode {
    explicit StackNode(MessageStackHandlers* handlers)
        : handlers(handlers), cached_handlers(handlers) {}
    WeakPtr<MessageStackHandlers> handlers;
    MessageStackHandlers* cached_handlers = nullptr;
  };
  using Stack = std::vector<StackNode>;
  using Handlers = std::vector<WeakPtr<MessageStackHandlers>>;
  struct MessageInfo {
    MessageInfo() = default;
    MessageStackOrder order = MessageStackOrder::kTopDown;
    Callback<void()> clear_handler;
  };
  using Messages = absl::flat_hash_map<TypeKey*, MessageInfo>;

  MessageStackEndpoint(MessageStackOrder default_order,
                       std::unique_ptr<MessageEndpoint> endpoint);

  template <typename Message>
  Messages::iterator GetMessageInfo() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Handlers GetHandlers(TypeKey* key) ABSL_LOCKS_EXCLUDED(mutex_);
  void HandleMessage(MessageEndpointId from, TypeKey* key, const void* message)
      ABSL_LOCKS_EXCLUDED(mutex_);

  const MessageStackOrder default_order_ = MessageStackOrder::kTopDown;
  const std::unique_ptr<MessageEndpoint> endpoint_;
  mutable absl::Mutex mutex_;
  Stack stack_ ABSL_GUARDED_BY(mutex_);
  Messages messages_ ABSL_GUARDED_BY(mutex_);
};

//------------------------------------------------------------------------------
// Template / inline implementation
//------------------------------------------------------------------------------

inline MessageStackEndpoint* MessageStackHandlers::GetStack() const {
  absl::MutexLock lock(&mutex_);
  return stack_;
}

template <typename Message>
void MessageStackHandlers::SetHandler(MessageStackHandler<Message> callback) {
  absl::MutexLock lock(&mutex_);
  auto& handler_info = handlers_[TypeKey::Get<Message>()];
  handler_info.callback = [callback = std::move(callback)](
                              MessageEndpointId from, const void* message) {
    return callback(from, *static_cast<const Message*>(message));
  };
  handler_info.register_message = [this]() {
    stack_->RegisterMessage<Message>({});
  };
  if (stack_ != nullptr) {
    stack_->RegisterMessage<Message>({});
  }
}

template <typename Message>
void MessageStackHandlers::ClearHandler() {
  absl::MutexLock lock(&mutex_);
  handlers_.erase(TypeKey::Get<Message>());
}

template <typename Message>
void MessageStackEndpoint::SetOrder(MessageStackOrder order) {
  absl::MutexLock lock(&mutex_);
  GetMessageInfo<Message>()->second.order = order;
}

template <typename Message>
MessageStackEndpoint::Messages::iterator
MessageStackEndpoint::GetMessageInfo() {
  auto key = TypeKey::Get<Message>();
  auto it = messages_.find(key);
  if (it != messages_.end()) {
    return it;
  }

  endpoint_->SetHandler<Message>(
      [this, key](MessageEndpointId from, const Message& message) {
        HandleMessage(from, key, &message);
      });

  MessageInfo message_info;
  message_info.order = default_order_;
  message_info.clear_handler = [this]() { endpoint_->ClearHandler<Message>(); };
  return messages_.insert({key, std::move(message_info)}).first;
}

}  // namespace gb

#endif  // GBITS_MESSAGE_MESSAGE_STACK_ENDPOINT_H_
