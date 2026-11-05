// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMGUI_GB_IMCONFIG_H_
#define GB_IMGUI_GB_IMCONFIG_H_

#include <cstdint>

// This file is used to configure Dear ImGui for the Game Bits engine.
// See third_party/imgui/imconfig.h for details on the various options.

#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS

// gb_imgui implements the test engine hooks (in imgui_pilot_hooks.cc), so
// ImGuiInstance can find items for a pilot. ImGui only calls them for a context
// that turns them on, as an instance with a pilot does, so otherwise each item
// and ID costs one comparison. As gb_imgui defines the hooks, Dear ImGui's own
// test engine can't be linked with it.
#define IMGUI_ENABLE_TEST_ENGINE

typedef void* ImFileHandle;
ImFileHandle ImFileOpen(const char* filename, const char* mode);
bool ImFileClose(ImFileHandle handle);
uint64_t ImFileGetSize(ImFileHandle handle);
uint64_t ImFileRead(void* data, uint64_t size, uint64_t count,
                    ImFileHandle handle);
uint64_t ImFileWrite(const void* data, uint64_t size, uint64_t count,
                     ImFileHandle handle);

#endif  // GB_IMGUI_GB_IMCONFIG_H_
