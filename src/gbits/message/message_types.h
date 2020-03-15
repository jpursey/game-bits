#ifndef GBITS_MESSAGE_MESSAGE_TYPES_H_
#define GBITS_MESSAGE_MESSAGE_TYPES_H_

namespace gb {

class MessageDispatcher;
class MessageEndpoint;
class MessageSystem;

// This is an internal class used to tag and enforce that functions internal to
// the message package may only be called by other classes in the package.
class MessageInternal final {
 public:
  ~MessageInternal() = default;
  MessageInternal(const MessageInternal&) = default;
  MessageInternal& operator=(const MessageInternal&) = default;

 private:
  friend class MessageSystem;
  friend class MessageEndpoint;
  friend class MessageDispatcher;

  MessageInternal() = default;
};

}  // namespace gb

#endif  // GBITS_MESSAGE_MESSAGE_TYPES_H_