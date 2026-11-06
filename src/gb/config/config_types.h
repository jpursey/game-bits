// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CONFIG_CONFIG_TYPES_H_
#define GB_CONFIG_CONFIG_TYPES_H_

#include "gb/base/access_token.h"

namespace gb {

class ConfigLocationTest;
class TextConfigBuilder;

// Internal access token for functions only callable within the config module.
GB_BEGIN_ACCESS_TOKEN(ConfigInternal)
friend class ConfigLocationTest;
friend class TextConfigBuilder;
GB_END_ACCESS_TOKEN()

}  // namespace gb

#endif  // GB_CONFIG_CONFIG_TYPES_H_
