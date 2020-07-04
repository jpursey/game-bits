#ifndef GBITS_MESSAGE_MESSAGE_TYPES_H_
#define GBITS_MESSAGE_MESSAGE_TYPES_H_

#include "gbits/base/access_token.h"

namespace gb {

class MessageDispatcher;
class MessageEndpoint;
class MessageStackEndpoint;
class MessageStackHandlers;
class MessageSystem;

GB_DEFINE_ACCESS_TOKEN(MessageInternal, class MessageDispatcher,
                       class MessageEndpoint, class MessageStackEndpoint,
                       class MessageStackHandlers, class MessageSystem);

}  // namespace gb

#endif  // GBITS_MESSAGE_MESSAGE_TYPES_H_