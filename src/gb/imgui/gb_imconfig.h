// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef IMGUI_GB_IMCONFIG_H_
#define IMGUI_GB_IMCONFIG_H_

#include <stdint.h>

// This file is used to configure Dear ImGui for the Game Bits engine.
// See third_party/imgui/imconfig.h for details on the various options.

#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS

typedef void* ImFileHandle;
ImFileHandle ImFileOpen(const char* filename, const char* mode);
bool ImFileClose(ImFileHandle handle);
uint64_t ImFileGetSize(ImFileHandle handle);
uint64_t ImFileRead(void* data, uint64_t size, uint64_t count,
                    ImFileHandle handle);
uint64_t ImFileWrite(const void* data, uint64_t size, uint64_t count,
                     ImFileHandle handle);

#endif  // IMGUI_GB_IMCONFIG_H_
