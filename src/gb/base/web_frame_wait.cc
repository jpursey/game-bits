// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/frame_wait.h"

#include <emscripten.h>

namespace gb {
namespace {

// These link in any web build, but can only be called where the program was
// linked with JSPI (as a page is), which emscripten_has_asyncify() says.

EM_ASYNC_JS(void, AwaitAnimationFrame, (), {
  await new Promise(resolve => requestAnimationFrame(resolve));
});

// A message posted to a MessageChannel arrives in the browser's next task,
// without the 4 ms that browsers clamp a nested setTimeout(0) to.
EM_ASYNC_JS(void, AwaitMessage, (), {
  await new Promise(resolve => {
    const channel = new MessageChannel();
    channel.port1.onmessage = () => {
      channel.port1.close();
      resolve();
    };
    channel.port2.postMessage(null);
  });
});

bool CanWait() { return emscripten_has_asyncify() != 0; }

}  // namespace

bool WaitForNextFrame() {
  if (!CanWait()) {
    return false;
  }
  AwaitAnimationFrame();
  return true;
}

void YieldBetweenFrames() {
  if (!CanWait()) {
    return;
  }
  AwaitMessage();
}

}  // namespace gb
