// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_MESSAGE_MESSAGE_TYPES_H_
#define GB_MESSAGE_MESSAGE_TYPES_H_

#include "gb/base/access_token.h"

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

#endif  // GB_MESSAGE_MESSAGE_TYPES_H_