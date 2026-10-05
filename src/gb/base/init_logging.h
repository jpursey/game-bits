// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_INIT_LOGGING_H_
#define GB_BASE_INIT_LOGGING_H_

namespace gb {

// Initializes Abseil logging, so messages go to their log sinks and stderr
// shows only those at or above its threshold, rather than all of them.
//
// Use this rather than absl::InitializeLog(), which may only be called once in
// a process. This may be called any number of times, from any thread.
void InitLogging();

}  // namespace gb

#endif  // GB_BASE_INIT_LOGGING_H_
