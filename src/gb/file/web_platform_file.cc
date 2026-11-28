// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/platform_file.h"

namespace gb::internal {

// Names pass through JavaScript strings, which Node converts to and from UTF-8
// with U+FFFD in place of bytes that aren't valid, so every name is UTF-8. A
// name another program made that isn't UTF-8 is listed with U+FFFD in it, and
// that name doesn't open it.
std::filesystem::path GetNonUtf8Name() { return {}; }

}  // namespace gb::internal
