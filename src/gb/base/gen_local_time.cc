// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/local_time.h"

namespace gb {

absl::TimeZone GetLocalTimeZone() { return absl::LocalTimeZone(); }

}  // namespace gb
